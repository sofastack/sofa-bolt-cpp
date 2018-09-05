// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/17.
//

#include "http_protocol.h"
#include <unordered_map>
#include <sstream>
#include "common/io_buffer.h"
#include "common/log.h"
#include "protocol/http/http_request.h"
#include "protocol/http/http_response.h"

namespace antflash {
namespace http {

struct HttpHeaderData {
    using HeaderMap = std::unordered_map<size_t, std::string>;
    HttpHeaderData() {
        _header_map.reserve(HTTP_HEADER_KEY_SIZE);
    }
    void setHeader(EHttpHeaderKey key, const std::string& value) {
        auto p = _header_map.emplace(key, value);
        if (!p.second) {
            p.first->second = value;
        }
    }

    HeaderMap _header_map;
};

class HttpInternalRequest {
public:
    HttpInternalRequest(const HttpRequest& request, size_t request_id) :
            _request(request) {
        std::stringstream ss;
        ss << request_id;
        _header.setHeader(LOG_ID, ss.str());
        _header.setHeader(CONNECTION, "keep-alive");
    }

    bool serialize(IOBuffer&);
private:
    HttpHeaderData _header;
    const HttpRequest& _request;
};

bool HttpInternalRequest::serialize(antflash::IOBuffer& buffer) {

    static const char* s_http_line_end = "\r\n";

    std::stringstream http_ss;
    http_ss << HttpHeader::getMethod(_request._method) << " ";
    http_ss << _request._uri.path();
    http_ss << " HTTP/1.1"<< s_http_line_end;
    if (_request._method != EHttpMethod::HTTP_METHOD_GET) {
        http_ss << "Content-Length: " << _request.attach_size() << s_http_line_end;
    }
    if (!_request._attch_type.empty()) {
        http_ss << "Content-Type: " << _request._attch_type << s_http_line_end;
    }

    http_ss << "Host: " << _request._uri.host()
            << ":" << _request._uri.port() << s_http_line_end;
    http_ss << "Accept: */*" << s_http_line_end;
    http_ss << "User-Agent: curl/7.0" << s_http_line_end;

    for (auto& p : _header._header_map) {
        http_ss << HttpHeader::getHeaderKey((EHttpHeaderKey)p.first)
                << ": " << p.second << s_http_line_end;
    }

    http_ss << s_http_line_end;

    buffer.append(http_ss.str());

    if (_request._data) {
        //FIXME copy iobuffer after refine iobuffer
        buffer.append(std::move(*_request._data));
    }

    LOG_INFO("http info:\n{}", buffer.to_string());

    return true;
}

bool assembleHttpRequest(
        const RequestBase &req,
        size_t request_id, IOBuffer& buffer) {
    auto http = static_cast<const HttpRequest*>(&req);
    if (!http->uri().parse_ok()) {
        LOG_ERROR("uri parse fail");
        return false;
    }
    HttpInternalRequest request(*http, request_id);
    return request.serialize(buffer);
}

EResParseResult parseHttpProtocol (
        IOBuffer& buffer, size_t& data_size,
        size_t& request_id, void** p) {
    return HttpResponse::checkHeader(buffer, data_size, request_id, p);
}

EResParseResult parseHttpResponse(
        ResponseBase& rsp, IOBuffer& buffer, void* addition_data) {
    auto http = static_cast<HttpResponse*>(&rsp);
    http->setHttpParser(addition_data);
    return rsp.deserialize(buffer);
}

bool assembleHeartbeatRequest(IOBuffer& buffer) {
    return true;
}

size_t converseHttpRequest(size_t request_id) {
    return request_id;
}

}
}
