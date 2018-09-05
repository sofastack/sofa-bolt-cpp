// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/2.
// Brief

#ifndef RPC_INCLUDE_ENDPOINT_H
#define RPC_INCLUDE_ENDPOINT_H

#include <netinet/in.h>
#include <string>

namespace antflash {

static constexpr in_addr IP_ANY = {INADDR_ANY};
static constexpr in_addr IP_NONE = {INADDR_NONE};

struct EndPoint {
    EndPoint() : ip(IP_ANY), port(0) {}
    EndPoint(const in_addr &i, int p) : ip(i), port(p) {}
    EndPoint(const sockaddr_in& in) : ip(in.sin_addr), port(in.sin_port) {}
    EndPoint(const EndPoint& right) : ip(right.ip), port(right.port) {}

    bool parseFromString(const char *str);
    std::string ipToStr() const;

    in_addr ip;
    int port;
};


}

#endif  //RPC_INCLUDE_ENDPOINT_H
