// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/9.
//

#include "loop.h"
#include <sys/epoll.h>
#include <errno.h>
#include "common/common_defines.h"

namespace antflash {

using ScheduleHandler = std::function<void()>;

struct LoopInternalData {
    struct epoll_event events[MAX_POLL_EVENT];
};

Loop::Loop() {

}

Loop::~Loop() {

}

bool Loop::init() {
    _backend_fd = epoll_create(1024*1024);


    if (!base::set_close_on_exec(_backend_fd.fd())) {
        return false;
    }

    if (_backend_fd.fd() == -1) {
        return false;
    }

    _data.reset(new LoopInternalData);
    return true;
}

void Loop::destroy() {

}

void Loop::loop_once() {
    auto actives = epoll_wait(_backend_fd.fd(), _data->events, MAX_POLL_EVENT, -1);

    if (actives == -1 && errno != EINTR) {
        //ERROR
        return;
    }

    for (auto i = 0; i < actives; ++i) {
        struct epoll_event& ke = _data->events[i];
        if (ke.data.ptr) {
            auto handler = static_cast<ScheduleHandler*>(ke.data.ptr);
            (*handler)();
        }
    }
}

bool Loop::add_event(int fd, int events, void* handler) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events | EPOLLET;
    ev.data.ptr = handler;

    return 0 == epoll_ctl(_backend_fd.fd(), EPOLL_CTL_ADD, fd, &ev);
}

void Loop::remove_event(int fd, int events) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events | EPOLLET;
    ev.data.ptr = nullptr;

    epoll_ctl(_backend_fd.fd(), EPOLL_CTL_DEL, fd, &ev);
}

}
