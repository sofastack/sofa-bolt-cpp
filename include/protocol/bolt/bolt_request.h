// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/4.
//

#ifndef RPC_INCLUDE_BOLT_REQUEST_H
#define RPC_INCLUDE_BOLT_REQUEST_H

#include <string>
#include <google/protobuf/message.h>
#include "protocol/request_base.h"
#include "common/common_defines.h"

namespace antflash {

namespace bolt {
class BoltInternalRequest;
}

class BoltRequest final : public RequestBase {
friend class bolt::BoltInternalRequest;
public:
    BoltRequest() : _data_type(EDataType::NONE) {
        _data.str = nullptr;
    }

    virtual ~BoltRequest() {}

    inline BoltRequest& service(const std::string& service) {
        _service = service;
        return *this;
    }
    inline BoltRequest& method(const std::string& method) {
        _method = method;
        return *this;
    }
    inline BoltRequest& traceId(const std::string& id) {
        _trace_id = id;
        return *this;
    }
    inline BoltRequest& data(const std::string& data) {
        _data.str = &data;
        _data_type = EDataType::STRING;
        return *this;
    }
    inline BoltRequest& data(const char* data) {
        _data.c_str = data;
        _data_type = EDataType::CSTRING;
        return *this;
    }
    inline BoltRequest& data(const google::protobuf::Message& data) {
        _data.proto = &data;
        _data_type = EDataType::PROTOBUF;
        return *this;
    }

private:
    enum class EDataType {
        CSTRING,
        STRING,
        PROTOBUF,
        NONE
    };

    std::string _service;
    std::string _method;
    std::string _trace_id;

    union {
        const char* c_str;
        const std::string* str;
        const google::protobuf::Message* proto;
    } _data;

    EDataType _data_type;
};

}

#endif //RPC_INCLUDE_BOLT_REQUEST_H
