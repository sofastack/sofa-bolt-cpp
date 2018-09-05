// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/4.
//

#ifndef RPC_PROTOCOL_BOLT_PROTOCOL_H
#define RPC_PROTOCOL_BOLT_PROTOCOL_H

#include <iostream>
#include <memory>
#include "protocol/request_base.h"
#include "protocol/response_base.h"

namespace antflash {
namespace bolt {

bool assembleBoltRequest(const RequestBase& req, size_t request_id, IOBuffer& buffer);
EResParseResult parseBoltProtocol(IOBuffer&, size_t&, size_t&, void**);
EResParseResult parseBoltResponse(ResponseBase& rsp, IOBuffer& buffer, void*);
bool assembleHeartbeatRequest(std::shared_ptr<RequestBase>&,
        std::shared_ptr<ResponseBase>&);
bool parseHeartbeatResponse(std::shared_ptr<ResponseBase>&);
size_t converseBoltRequest(size_t request_id);
}
}

#endif //RPC_PROTOCOL_BOLT_PROTOCOL_H
