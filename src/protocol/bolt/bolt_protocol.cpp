// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/4.
//

#include "bolt_protocol.h"
#include <arpa/inet.h>
#include "protocol/bolt/bolt_request.h"
#include "protocol/bolt/bolt_response.h"
#include "common/io_buffer.h"
#include "common/log.h"

namespace antflash {
namespace bolt {

struct [[gnu::packed]] BoltRequestHeader {
    const uint8_t proto = BOLT_PROTOCOL_TYPE;
    const uint8_t type = BOLT_PROTOCOL_REQUEST;
    uint16_t cmdcode;
    uint8_t ver2 = BOLT_PROTOCOL_VER2;
    uint32_t request_id;
    const uint8_t codec = BOLT_PROTOCOL_CODEC_PB;

    uint32_t timeout;
    uint16_t class_len;
    uint16_t header_len;
    uint32_t content_len;

    void hton() {
        cmdcode = htons(cmdcode);
        request_id = htonl(request_id);
        timeout = htonl(timeout);
        class_len = htons(class_len);
        header_len = htons(header_len);
        content_len = htonl(content_len);
    }
};

class BoltInternalRequest {
public:
    BoltInternalRequest(
            const BoltRequest& req, size_t request_id) :
            _request(req) {
        initHeader();
        _header.request_id = request_id;
        if (req._data_type == BoltRequest::EDataType::NONE) {
            _header.cmdcode = BOLT_PROTOCOL_CMD_HEARTBEAT;
        }
    }

    ~BoltInternalRequest() {}

    bool serialize(IOBuffer& buffer);

    void initHeader() noexcept {
        _header.cmdcode = BOLT_PROTOCOL_CMD_REQUEST;
        _header.request_id = 0;
        _header.timeout = -1;
        _header.class_len = 0;
        _header.header_len = 0;
        _header.content_len = 0;
    }

    BoltRequestHeader _header;
    const BoltRequest& _request;
};

class BoltHeartbeatBuffer {
public:
    BoltHeartbeatBuffer() : _req(new BoltRequest) {
    }

    const std::shared_ptr<BoltRequest>& data() const {
        return _req;
    }
private:
    std::shared_ptr<BoltRequest> _req;
};

static uint32_t appendBoltHeaderKV(
        IOBuffer &buffer,
        const char *key, uint32_t key_size,
        const char *value, uint32_t value_size) {
    constexpr uint32_t KEY_VALUE_SIZE_BTYES = sizeof(uint32_t) * 2;

    uint32_t key_size_network = htonl(key_size);
    buffer.append(&key_size_network, sizeof(uint32_t));//key size
    buffer.append(key, key_size);//key
    uint32_t value_size_network = htonl(value_size);
    buffer.append(&value_size_network, sizeof(uint32_t));//value size
    buffer.append(value, value_size);

    return KEY_VALUE_SIZE_BTYES + key_size + value_size;
}

bool BoltInternalRequest::serialize(IOBuffer& buffer) {
    if (_header.cmdcode == BOLT_PROTOCOL_CMD_HEARTBEAT) {
        _header.hton();
        buffer.append((void*)&_header, sizeof(_header));
        return true;
    }

    IOBuffer buffer_body;

    //class name
    constexpr char class_name[] = "com.alipay.sofa.rpc.core.request.SofaRequest";
    constexpr uint32_t class_name_size = sizeof(class_name) - 1;
    _header.class_len = class_name_size;
    buffer_body.append(class_name, class_name_size);

    //service
    const char service_key[] = "service";
    constexpr uint32_t service_key_size = sizeof(service_key) - 1;
    _header.header_len += appendBoltHeaderKV(
            buffer_body,
            service_key, service_key_size,
            _request._service.data(),
            _request._service.size());

    //header
    const char sofa_service_key[] = "sofa_head_target_service";
    constexpr uint32_t sofa_service_key_size = sizeof(sofa_service_key) - 1;
    _header.header_len += appendBoltHeaderKV(
            buffer_body, sofa_service_key, sofa_service_key_size,
            _request._service.data(),
            _request._service.size());

    constexpr char method_key[] = "sofa_head_method_name";
    constexpr uint32_t method_key_size = sizeof(method_key) - 1;
    _header.header_len += appendBoltHeaderKV(
            buffer_body, method_key, method_key_size,
            _request._method.data(),
            _request._method.size());

    constexpr char trace_id_key[] = "rpc_trace_context.sofaTraceId";
    constexpr uint32_t trace_id_key_size = sizeof(trace_id_key) - 1;
    _header.header_len += appendBoltHeaderKV(
            buffer_body, trace_id_key, trace_id_key_size,
            _request._trace_id.data(),
            _request._trace_id.size());

    if (_request._data_type == BoltRequest::EDataType::PROTOBUF
        && _request._data.proto) {
        //TODO compress if needed
        IOBufferZeroCopyOutputStream wrapper(&buffer_body);
        _request._data.proto->SerializeToZeroCopyStream(&wrapper);
        _header.content_len = buffer_body.size()
                              - _header.class_len - _header.header_len;
    } else if (_request._data_type == BoltRequest::EDataType::STRING
               && _request._data.str) {
        _header.content_len = _request._data.str->length();
        buffer_body.append(_request._data.str->data(),
                           _request._data.str->length());
    } else if (_request._data_type == BoltRequest::EDataType::CSTRING
               && _request._data.c_str) {
        _header.content_len = strlen(_request._data.c_str);
        buffer.append(_request._data.c_str);
    }

    _header.hton();
    buffer.append((void*)&_header, sizeof(_header));
    buffer.append(std::move(buffer_body));

    return true;
}

bool assembleBoltRequest(const RequestBase &req, size_t request_id, IOBuffer& buffer) {
    const BoltRequest* breq = static_cast<const BoltRequest*>(&req);
    BoltInternalRequest inner_request(*breq, request_id);
    return inner_request.serialize(buffer);
}

EResParseResult parseBoltProtocol (
        IOBuffer& buffer, size_t& data_size, size_t& request_id,  void**/*unused*/) {
    return BoltResponse::checkHeader(buffer, data_size, request_id);
}

EResParseResult parseBoltResponse(ResponseBase& rsp, IOBuffer& buffer, void*/*unused*/) {
    return rsp.deserialize(buffer);
}


bool assembleHeartbeatRequest(std::shared_ptr<RequestBase>& req,
                              std::shared_ptr<ResponseBase>& rsp) {
    //only initialize once
    static BoltHeartbeatBuffer s_heartbeat;
    req = s_heartbeat.data();
    rsp.reset(new BoltResponse());
    return true;
}
bool parseHeartbeatResponse(std::shared_ptr<ResponseBase>& res) {
    if (res) {
        BoltResponse* response = static_cast<BoltResponse*>(res.get());
        return response->isHeartbeat() && response->status() == BoltResponse::SUCCESS;
    }
    return false;
}

size_t converseBoltRequest(size_t request_id) {
    return size_t(uint32_t(request_id));
}

}
}
