// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/11.
//

#include "protocol/bolt/bolt_response.h"
#include <arpa/inet.h>
#include <google/protobuf/io/coded_stream.h>
#include "common/io_buffer.h"
#include "common/log.h"

namespace antflash {

void BoltResponse::BoltHeader::ntoh() {
    cmdcode = ntohs(cmdcode);
    request_id = ntohl(request_id);
    status = ntohs(status);
    class_len = ntohs(class_len);
    header_len = ntohs(header_len);
    content_len = ntohl(content_len);
}

static std::unordered_map<uint16_t, std::string> s_rsp_error_info = {
        {BoltResponse::SUCCESS, "success"},
        {BoltResponse::ERROR, "error"},
        {BoltResponse::SERVER_EXCEPTION, "server exception"},
        {BoltResponse::UNKNOWN, "unknown"},
        {BoltResponse::SERVER_THREADPOOL_BUSY, "server thread pool busy"},
        {BoltResponse::ERROR_COMM, "error communication"},
        {BoltResponse::NO_PROCESSOR, "no processor"},
        {BoltResponse::TIMEOUT, "timeout"},
        {BoltResponse::CLIENT_SEND_ERROR, "client send error"},
        {BoltResponse::CODEC_EXCEPTION, "codec exception"},
        {BoltResponse::CONNECTION_CLOSED, "connection closed"},
        {BoltResponse::SERVER_SERIAL_EXCEPTION, "server serial exception"},
        {BoltResponse::SERVER_DESERIAL_EXCEPTION, "server deserial exception"}
};

const std::string& BoltResponse::msg() const {
    static const std::string unknown = "unknown";
    auto itr = s_rsp_error_info.find(_header.status);
    if (itr == s_rsp_error_info.end()) {
        return unknown;
    }
    return itr->second;
}

EResParseResult BoltResponse::deserialize(IOBuffer &buffer) noexcept {
    auto cn = buffer.cut(&_header, sizeof(BoltHeader));
    if (cn < sizeof(BoltHeader)) {
        LOG_ERROR("not enough data for header");
        return EResParseResult::PARSE_NOT_ENOUGH_DATA;
    }

    _header.ntoh();

    if (_header.proto != BOLT_PROTOCOL_TYPE) {
        LOG_ERROR("proto type is {}, not {}", _header.proto, BOLT_PROTOCOL_TYPE);
        return EResParseResult::PARSE_ERROR;
    }

    if (_header.type != BOLT_PROTOCOL_RESPONSE) {
        LOG_ERROR("message type is {}, not {}", _header.type, BOLT_PROTOCOL_RESPONSE);
        return EResParseResult::PARSE_ERROR;
    }

    if (_header.cmdcode == BOLT_PROTOCOL_CMD_HEARTBEAT) {
        LOG_DEBUG("heartbeat received");
        return EResParseResult::PARSE_OK;
    }

    if (_header.cmdcode != BOLT_PROTOCOL_CMD_RESPONSE) {
        LOG_ERROR("protocol cmd {}, not {}", (int)_header.cmdcode, BOLT_PROTOCOL_CMD_RESPONSE);
        return EResParseResult::PARSE_ERROR;
    }

    LOG_DEBUG("receive data status: {}", (int)_header.status);
    if (_header.status != EBoltResponseStatus::SUCCESS) {
        //如果status不是SUCCESS，则header和content内容不一定能解析，直接丢弃
        auto data_size = _header.class_len
                         + _header.header_len
                         + _header.content_len;
        buffer.pop_front(data_size);

        return EResParseResult::PARSE_OK;
    }

    _class_name.resize(_header.class_len);
    cn = buffer.cut(&_class_name[0], _header.class_len);
    if (cn < _header.class_len) {
        LOG_ERROR("parse class name fail");
        return EResParseResult::PARSE_NOT_ENOUGH_DATA;
    }

    uint16_t left_size = _header.header_len;
    while (left_size > 0) {
        std::string key_str;
        uint32_t header_key_size = 0;
        cn = buffer.cut(&header_key_size, sizeof(uint32_t));
        if (cn < sizeof(uint32_t)) {
            LOG_ERROR("parse header key size fail");
            return EResParseResult::PARSE_NOT_ENOUGH_DATA;
        }
        header_key_size = ntohl(header_key_size);
        left_size -= sizeof(uint32_t);

        if (header_key_size > 0) {
            key_str.resize(header_key_size);
            cn = buffer.cut(&key_str[0], header_key_size);
            if (cn < header_key_size) {
                LOG_ERROR("parse header key name fail");
                return EResParseResult::PARSE_NOT_ENOUGH_DATA;
            }
            left_size -= header_key_size;
        }

        uint32_t header_value_size = 0;
        std::string value_str;
        cn = buffer.cut(&header_value_size, sizeof(uint32_t));
        if (cn < sizeof(uint32_t)) {
            LOG_ERROR("parse header value size fail");
            return EResParseResult::PARSE_NOT_ENOUGH_DATA;
        }
        header_value_size = ntohl(header_value_size);
        left_size -= sizeof(uint32_t);

        if (header_value_size > 0) {
            value_str.resize(header_value_size);
            cn = buffer.cut(&value_str[0], header_value_size);
            if (cn < header_value_size) {
                LOG_ERROR("parse header value name fail");
                return EResParseResult::PARSE_NOT_ENOUGH_DATA;
            }
            left_size -= header_value_size;
        }
        _header_map.emplace(std::move(key_str), std::move(value_str));
    }

    if (_header.content_len > 0) {
        if (_data_type == EDataType::STRING) {
            _data.str->resize(_header.content_len);
            cn = buffer.cut(&(*_data.str)[0], _header.content_len);
            if (cn < _header.content_len) {
                LOG_ERROR("parse string content fail");
                return EResParseResult::PARSE_NOT_ENOUGH_DATA;
            }
        } else if (_data_type == EDataType::PROTOBUF) {
            IOBufferZeroCopyInputStream stream(buffer);
            google::protobuf::io::ZeroCopyInputStream *input = &stream;
            google::protobuf::io::CodedInputStream decoder(input);
            if (!(_data.proto->ParseFromCodedStream(&decoder)
                  && decoder.ConsumedEntireMessage())) {
                LOG_ERROR("parse protobuf content fail, data info:{}", buffer.to_string());
                return EResParseResult::PARSE_ERROR;
            }
            if (stream.ByteCount() != _header.content_len) {
                LOG_ERROR("parse protobuf content fail, size not correct");
                return EResParseResult::PARSE_ERROR;
            }
            buffer.pop_front(_header.content_len);
        }
    }

    return EResParseResult::PARSE_OK;
}

EResParseResult BoltResponse::checkHeader(
        const IOBuffer &buffer,
        size_t& data_size,
        size_t& request_id) {
    constexpr size_t MAX_BOLT_BODY_SIZE = 64 * 1024 * 1024;

    BoltHeader header;
    auto cn = buffer.copy_to(&header, sizeof(BoltHeader));
    header.ntoh();

    if (cn < sizeof(BoltHeader)) {
        return EResParseResult::PARSE_NOT_ENOUGH_DATA;
    }

    request_id = header.request_id;
    data_size = header.class_len + header.header_len + header.content_len;
    if (data_size > MAX_BOLT_BODY_SIZE) {
        return EResParseResult::PARSE_ERROR;
    }
    data_size += sizeof(header);
    if (data_size > buffer.size()) {
        return EResParseResult::PARSE_NOT_ENOUGH_DATA;
    }

    return EResParseResult::PARSE_OK;
}

}
