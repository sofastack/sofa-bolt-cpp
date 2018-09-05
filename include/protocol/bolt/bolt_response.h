// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/11.
//

#ifndef RPC_INCLUDE_BOLT_RESPONSE_H
#define RPC_INCLUDE_BOLT_RESPONSE_H

#include <string>
#include <unordered_map>
#include <google/protobuf/message.h>
#include "protocol/response_base.h"
#include "common/common_defines.h"

namespace antflash {

class BoltResponse final : public ResponseBase {
public:
    enum EBoltResponseStatus {
        SUCCESS = 0x0000,
        ERROR =  0x0001,
        SERVER_EXCEPTION = 0x0002,
        UNKNOWN = 0x0003,
        SERVER_THREADPOOL_BUSY = 0x0004,
        ERROR_COMM = 0x0005,
        NO_PROCESSOR = 0x0006,
        TIMEOUT = 0x0007,
        CLIENT_SEND_ERROR = 0x0008,
        CODEC_EXCEPTION = 0x0009,
        CONNECTION_CLOSED = 0x0010,
        SERVER_SERIAL_EXCEPTION = 0x0011,
        SERVER_DESERIAL_EXCEPTION = 0x0012
    };

    BoltResponse() {
        _data_type = EDataType::NONE;
    }

    BoltResponse(google::protobuf::Message& message) {
        _data.proto = &message;
        _data_type = EDataType::PROTOBUF;
    }

    BoltResponse(std::string& message) {
        _data.str = &message;
        _data_type = EDataType::STRING;
    }

    ~BoltResponse() {
    }

    EResParseResult deserialize(IOBuffer &buffer) noexcept override;
    static EResParseResult checkHeader(
            const IOBuffer &buffer,
            size_t& data_size,
            size_t& request_id);

    bool isHeartbeat() const {
        return _header.cmdcode == BOLT_PROTOCOL_CMD_HEARTBEAT;
    }

    uint16_t status() const {
        return _header.status;
    }

    const std::string& msg() const;

private:
    enum class EDataType {
        STRING = 0,
        PROTOBUF,
        NONE
    };

    struct [[gnu::packed]] BoltHeader {
        uint8_t proto;
        uint8_t type;
        uint16_t cmdcode;
        uint8_t ver2;
        uint32_t request_id;
        uint8_t codec;

        uint16_t status;
        uint16_t class_len;
        uint16_t header_len;
        uint32_t content_len;

        void ntoh();
    };

    mutable BoltHeader _header;
    std::string _class_name;
    std::unordered_map<std::string, std::string> _header_map;

    union {
        std::string* str;
        google::protobuf::Message* proto;
    } _data;

    EDataType _data_type;
};

}
#endif //RPC_INCLUDE_BOLT_RESPONSE_H
