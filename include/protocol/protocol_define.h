// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/3.
//

#ifndef RPC_INCLUDE_PROTOCOL_DEFINE_H
#define RPC_INCLUDE_PROTOCOL_DEFINE_H

#include <iostream>
#include <memory>
#include "request_base.h"
#include "response_base.h"

namespace antflash {

class IOBuffer;

enum class EProtocolType {
    PROTOCOL_BOLT,
    PROTOCOL_HTTP,
    PROTOCOL_TOTAL
};

struct Protocol {
public:
    //Assemble request with unique request id in process to io buffer
    using AssembleRequestFn = bool(*)(const RequestBase&, size_t request_id, IOBuffer&);
    AssembleRequestFn assemble_request_fn;

    //Parse io buffer data to check if data fits specific protocol.
    //If ok, returns total size of the response data and unique request id.
    //If io buffer data is copied in this method, response data means the whole data
    //and parse_response_fn will parse from begin.
    //Else response data means left data parse_response_fn need to parse, and parsed data
    //is transfer by @data
    using ParseProtocolFn = EResParseResult (*) (IOBuffer&, size_t&, size_t&, void** data);
    ParseProtocolFn parse_protocol_fn;

    //Parse the whole response data from io buffer, @data contains parsed data by parse_protocol_fn
    using ParseResponseFn = EResParseResult (*)(ResponseBase&, IOBuffer&, void* data);
    ParseResponseFn parse_response_fn;

    //Assemble heartbeat request and response
    using AssembleHeartbeatFn = bool(*)(
            std::shared_ptr<RequestBase>&, std::shared_ptr<ResponseBase>&);
    AssembleHeartbeatFn assemble_heartbeat_fn;

    //Verify heartbeat response
    using ParseHeartbeatFn = bool(*)(std::shared_ptr<ResponseBase>&);
    ParseHeartbeatFn parse_heartbeat_fn;

    using ConverseRequestIdFn = size_t (*)(size_t);
    ConverseRequestIdFn converse_request_fn;

    EProtocolType type;
};

const Protocol* getProtocol(EProtocolType type);

}

#endif //RPC_INCLUDE_PROTOCOL_DEFINE_H
