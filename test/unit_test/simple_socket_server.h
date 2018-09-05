// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/25.
//

#ifndef RPC_SIMPLE_SOCKET_SERVER_H
#define RPC_SIMPLE_SOCKET_SERVER_H

#include <string>

namespace antflash {

class SimpleSocketServer {
public:
    SimpleSocketServer() : _exit(false) {}

    bool run(int port);

    void stop() {
        _exit = true;
    }

private:
    bool _exit;
};

}

#endif //RPC_SIMPLE_SOCKET_SERVER_H
