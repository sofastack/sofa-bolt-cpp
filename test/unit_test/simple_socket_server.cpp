// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/25.
//

#include "simple_socket_server.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <thread>
#include <chrono>

namespace antflash {

bool SimpleSocketServer::run(int port) {

    int ss = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_sockaddr;
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_port = htons(port);

    server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(ss, (struct sockaddr* ) &server_sockaddr, sizeof(server_sockaddr))==-1) {
        return false;
    }
    if(listen(ss, 20) == -1) {
        return false;
    }

    struct sockaddr_in client_addr;
    socklen_t length = sizeof(client_addr);
    ///成功返回非负描述字，出错返回-1
    auto conn = accept(ss, (struct sockaddr*)&client_addr, &length);
    if( conn < 0 ) {
        return false;
    }

    //char buffer[1024];
    while(!_exit) {
        //memset(buffer, 0 ,sizeof(buffer));
        //int len = recv(conn, buffer, sizeof(buffer), 0);
        //if(strcmp(buffer, "exit\n") == 0) break;
        //必须要有返回数据， 这样才算一个完整的请求
        //send(conn, buffer, len , 0);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    close(conn);
    close(ss);
    return true;
}

}
