// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/17.
//

#include "protocol/http/http_response.h"
#include <fstream>
#include <string>
#include <unordered_map>
#include "http_parser.h"
#include "common/io_buffer.h"
#include "common/log.h"

namespace antflash {

static const std::unordered_map<size_t, std::string> s_http_status = {
        { EHttpStatus::HTTP_STATUS_CONTINUE, "Continue" },
        { EHttpStatus::HTTP_STATUS_OK, "OK" },
        { EHttpStatus::HTTP_STATUS_CREATED, "Created" },
        { EHttpStatus::HTTP_STATUS_ACCEPTED, "Accepted" },
        { EHttpStatus::HTTP_STATUS_BAD_REQUEST, "Bad Request" },
        { EHttpStatus::HTTP_STATUS_FORBIDDEN, "Forbidden" },
        { EHttpStatus::HTTP_STATUS_NOT_FOUND, "Not Found" },
};

enum HttpParserStage {
    HTTP_ON_MESSAGE_BEGIN,
    HTTP_ON_URL,
    HTTP_ON_STATUS,
    HTTP_ON_HEADER_FIELD,
    HTTP_ON_HEADER_VALUE,
    HTTP_ON_HEADERS_COMPLELE,
    HTTP_ON_BODY,
    HTTP_ON_MESSAGE_COMPLELE
};

struct HttpMessageInfo {
    HttpParserStage stage;
    std::string url;
    std::string cur_header;
    std::string body;
    std::unordered_map<std::string, std::string> header;
};

const http_parser_settings g_parser_settings = {
        HttpResponse::onMessageBegin,
        HttpResponse::onUrl,
        HttpResponse::onStatus,
        HttpResponse::onHeaderField,
        HttpResponse::onHeaderValue,
        HttpResponse::onHeadersComplete,
        HttpResponse::onBodyCb,
        HttpResponse::onMessageCompleteCb
};

int HttpResponse::onMessageBegin(struct http_parser* parser) {
    LOG_DEBUG("onMessageBegin:{}", fmt::ptr(parser));
    HttpMessageInfo* msg = static_cast<HttpMessageInfo*>(parser->data);
    msg->stage = HTTP_ON_MESSAGE_BEGIN;
    return 0;
}

int HttpResponse::onUrl(
        struct http_parser* parser, const char * data, size_t size) {
    HttpMessageInfo* msg = static_cast<HttpMessageInfo*>(parser->data);
    msg->stage = HTTP_ON_URL;
    msg->url.append(data, size);
    return 0;
}

int HttpResponse::onStatus(
        struct http_parser* parser, const char *, size_t) {
    LOG_DEBUG("onStatus:{}", fmt::ptr(parser));
    HttpMessageInfo* msg = static_cast<HttpMessageInfo*>(parser->data);
    msg->stage = HTTP_ON_STATUS;
    return 0;
}

int HttpResponse::onHeaderField(
        struct http_parser* parser, const char* data, size_t size) {
    LOG_DEBUG("onHeaderField:{}", fmt::ptr(parser));
    HttpMessageInfo* msg = static_cast<HttpMessageInfo*>(parser->data);
    if (msg->stage != HTTP_ON_HEADER_FIELD) {
        msg->stage = HTTP_ON_HEADER_FIELD;
        msg->cur_header.clear();
    }

    msg->cur_header.append(data, size);
    return 0;
}

int HttpResponse::onHeaderValue(
        struct http_parser* parser, const char* data, size_t size) {
    LOG_DEBUG("onHeaderValue:{}", fmt::ptr(parser));
    HttpMessageInfo* msg = static_cast<HttpMessageInfo*>(parser->data);
    if (msg->stage != HTTP_ON_HEADER_VALUE) {
        msg->stage = HTTP_ON_HEADER_VALUE;
        if (msg->cur_header.empty()) {
            LOG_ERROR("Header name is empty");
            return -1;
        }
    }

    std::string value(data, size);
    auto itr = msg->header.emplace(msg->cur_header, value);
    if (!itr.second) {
        itr.first->second.append(",");
        itr.first->second.append(value);
    }

    return 0;
}

int HttpResponse::onHeadersComplete(struct http_parser* parser) {
    LOG_DEBUG("onHeadersComplete:{}", fmt::ptr(parser));
    HttpMessageInfo* msg = static_cast<HttpMessageInfo*>(parser->data);
    msg->stage = HTTP_ON_HEADERS_COMPLELE;

    return 0;
}

int HttpResponse::onBodyCb(
        struct http_parser* parser, const char* data, size_t size) {
    LOG_DEBUG("onBodyCb:{}", fmt::ptr(parser));
    HttpMessageInfo* msg = static_cast<HttpMessageInfo*>(parser->data);
    msg->stage = HTTP_ON_BODY;
    msg->body.append(data, size);

    return 0;
}

int HttpResponse::onMessageCompleteCb(struct http_parser* parser) {
    LOG_DEBUG("onMessageCompleteCb:{}", fmt::ptr(parser));
    HttpMessageInfo* msg = static_cast<HttpMessageInfo*>(parser->data);
    msg->stage = HTTP_ON_MESSAGE_COMPLELE;

    return 0;
}

HttpResponse::~HttpResponse() {
    if (_parser) {
        HttpMessageInfo* msg =
                static_cast<HttpMessageInfo*>(_parser->data);
        delete msg;
        delete _parser;
    }
}

void HttpResponse::setHttpParser(void* p) {
    _parser = static_cast<struct http_parser*>(p);
}

EResParseResult HttpResponse::deserialize(IOBuffer& /*unused*/) noexcept {
    auto ret = EResParseResult::PARSE_OK;
    do {
        if (_parser == nullptr) {
            ret = EResParseResult::PARSE_ERROR;
            break;
        }
        auto info = (HttpMessageInfo*)_parser->data;
        if (info == nullptr ||
            info->stage != HTTP_ON_MESSAGE_COMPLELE) {
            ret = EResParseResult::PARSE_ERROR;
            break;
        }
        _url = std::move(info->url);
        _body = std::move(info->body);
        _header = std::move(info->header);
        delete info;
        _parser->data = nullptr;
    } while (0);

    delete _parser;
    _parser = nullptr;
    return ret;
}

EResParseResult HttpResponse::checkHeader(
        IOBuffer &buffer,
        size_t& data_size,
        size_t& request_id,
        void** p) {
    EResParseResult ret = EResParseResult::PARSE_NOT_ENOUGH_DATA;
    struct http_parser* parser = nullptr;
    HttpMessageInfo* info = nullptr;

    if (nullptr != p && nullptr != *p) {
        parser = (struct http_parser*)(*p);
        info = (HttpMessageInfo*)parser->data;
    } else {
        parser = new struct http_parser;
        http_parser_init(parser, HTTP_BOTH);
        info = new HttpMessageInfo;
        parser->data = info;
    }

    size_t nr = 0;
    for (size_t i = 0; i < buffer.slice_num(); ++i) {
        auto data = buffer.slice(i);
        if (data.second > 0) {
            nr += http_parser_execute(
                    parser, &g_parser_settings,
                    data.first, data.second);
            if (parser->http_errno != 0) {
                //FIXME http parse fail handle
                LOG_ERROR("http parse error:{}", (int)parser->http_errno);
                ret = EResParseResult::PARSE_ERROR;
                delete info;
                info = nullptr;
                delete parser;
                parser = nullptr;
                break;
            }
            if (info->stage == HTTP_ON_MESSAGE_COMPLELE) {
                ret = EResParseResult::PARSE_OK;
                break;
            }
        }
    }

    buffer.pop_front(nr);
    data_size = 0;

    if (p != nullptr) {
        *p = (void*)parser;
    } else {
        delete info;
        info = nullptr;
        delete parser;
        parser = nullptr;
    }

    return ret;
}

}
