// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/17.
//

#ifndef RPC_PROTOCOL_HTTP_PROTOCOL_H
#define RPC_PROTOCOL_HTTP_PROTOCOL_H

#include <iostream>
#include "protocol/request_base.h"
#include "protocol/response_base.h"

namespace antflash {
namespace http {

bool assembleHttpRequest(const RequestBase& req, size_t request_id, IOBuffer& buffer);
EResParseResult parseHttpProtocol(IOBuffer&, size_t&, size_t&, void**);
EResParseResult parseHttpResponse(ResponseBase& rsp, IOBuffer& buffer, void*);
bool assembleHeartbeatRequest(IOBuffer& buffer);
size_t converseHttpRequest(size_t request_id);

}
}


#endif //RPC_HTTP_PROTOCOL_H
