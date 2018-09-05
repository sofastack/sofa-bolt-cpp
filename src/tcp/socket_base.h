// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/2.
//

#ifndef RPC_TCP_SOCKET_BASE_H
#define RPC_TCP_SOCKET_BASE_H

#include <algorithm> //std::swap before c++11
#include <utility>   //std::swap in c++11
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "tcp/endpoint.h"

namespace antflash {

namespace base {
//RAII
class FdGuard {
public:
    FdGuard() : _fd(-1) {}

    FdGuard(int fd) : _fd(fd) {}

    ~FdGuard() {
        release();
    }

    FdGuard(const FdGuard &) = delete;

    FdGuard(FdGuard&& right) : _fd(right._fd) {
        right._fd = -1;
    }

    FdGuard& operator=(const FdGuard&) = delete;

    FdGuard& operator=(FdGuard&& right) {
        if (&right != this) {
            _fd = right._fd;
            right._fd = -1;
        }

        return *this;
    }

    FdGuard& operator=(int fd) {
        _fd = fd;
        return *this;
    }

    inline void swap(FdGuard &right) {
        std::swap(right._fd, _fd);
    }

    inline int fd() const {
        return _fd;
    }

    void release() {
        if (_fd >= 0) {
            close(_fd);
        }
        _fd = -1;
    }

    int handover() {
        int fd = _fd;
        _fd = -1;
        return fd;
    }

private:
    int _fd;
};

FdGuard create_socket();
bool prepare_socket(FdGuard& fd);
int connect(FdGuard& fd, EndPoint& remote);
bool connected(FdGuard& fd);

bool set_blocking(int fd);
bool set_non_blocking(int fd);
bool set_close_on_exec(int fd);
bool set_no_delay(int socket);

}
}

#endif //RPC_TCP_SOCKET_BASE_H
