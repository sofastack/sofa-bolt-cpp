// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/8.
//

#include "socket_base.h"
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

namespace antflash {
namespace base {

bool set_non_blocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    if (flags & O_NONBLOCK) {
        return true;
    }
    return 0 == fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

bool set_blocking(int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    if (flags & O_NONBLOCK) {
        return 0 == fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    }
    return true;
}

bool set_close_on_exec(int fd) {
    return 0 == fcntl(fd, F_SETFD, FD_CLOEXEC);
}

bool set_no_delay(int socket) {
    int flag = 1;
    return 0 == setsockopt(socket, IPPROTO_TCP,
                           TCP_NODELAY, (char*)&flag, sizeof(flag));
}


FdGuard create_socket() {
    return FdGuard(socket(AF_INET, SOCK_STREAM, 0));
}

bool prepare_socket(FdGuard& fd) {
#if defined(OS_MACOSX)
    const int value = 1;
    setsockopt(fd.fd(), SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(int));
#endif
    return set_non_blocking(fd.fd())
           && set_close_on_exec(fd.fd())
           && set_no_delay(fd.fd());
}

int connect(FdGuard& fd, EndPoint& remote) {
    struct sockaddr_in addr;
    bzero((char*)&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr = remote.ip;
    addr.sin_port = htons(remote.port);

    return connect(fd.fd(), (struct sockaddr*)&addr, sizeof(addr));
}

bool connected(FdGuard& fd) {
    if (fd.fd() < 0) {
        return false;
    }

    int err = 0;
    socklen_t errlen = sizeof(err);
    if (getsockopt(fd.fd(), SOL_SOCKET, SO_ERROR, &err, &errlen) < 0) {
        return false;
    }

    if (err != 0) {
        return false;
    }

    return true;
}

}
}