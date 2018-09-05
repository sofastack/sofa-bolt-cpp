// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/4.
//

#include "protocol/protocol_define.h"
#include <string>
#include "bolt/bolt_protocol.h"
#include "http/http_protocol.h"

namespace antflash {

static Protocol s_protocol[static_cast<size_t>(EProtocolType::PROTOCOL_TOTAL)];

void registerProtocol(EProtocolType type, const Protocol& protocol) {
    s_protocol[static_cast<size_t>(type)] = protocol;
}

const Protocol* getProtocol(EProtocolType type) {
    return &s_protocol[static_cast<size_t>(type)];
}

class GlobalProtocolInitializer {
public:
    GlobalProtocolInitializer() {
        Protocol bolt_protocol = {bolt::assembleBoltRequest,
                                  bolt::parseBoltProtocol,
                                  bolt::parseBoltResponse,
                                  bolt::assembleHeartbeatRequest,
                                  bolt::parseHeartbeatResponse,
                                  bolt::converseBoltRequest,
                                  EProtocolType::PROTOCOL_BOLT};

        registerProtocol(EProtocolType::PROTOCOL_BOLT, bolt_protocol);

        Protocol http_protocol = {http::assembleHttpRequest,
                                  http::parseHttpProtocol,
                                  http::parseHttpResponse,
                                  nullptr,
                                  nullptr,
                                  nullptr,
                                  EProtocolType::PROTOCOL_HTTP};
        registerProtocol(EProtocolType::PROTOCOL_HTTP, http_protocol);
    }

    ~GlobalProtocolInitializer() {}
};

static GlobalProtocolInitializer s_global_initializer;

}
