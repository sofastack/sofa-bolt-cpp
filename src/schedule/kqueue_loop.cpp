// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/9.
//

#include "loop.h"
#include <functional>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <poll.h>
#include <errno.h>
#include "common/common_defines.h"

namespace antflash {

using ScheduleHandler = std::function<void()>;

struct LoopInternalData {
    struct kevent events[MAX_POLL_EVENT];
};

Loop::Loop() {

}

Loop::~Loop() {

}

bool Loop::init() {
    _backend_fd = kqueue();

    if (_backend_fd.fd() == -1) {
        return false;
    }

    if (!base::set_close_on_exec(_backend_fd.fd())) {
        return false;
    }

    _data.reset(new LoopInternalData);
    return true;
}

void Loop::destroy() {

}

void Loop::loop_once() {
    auto actives = kevent(_backend_fd.fd(), nullptr, 0, _data->events, MAX_POLL_EVENT, nullptr);

    if (actives == -1 && errno != EINTR) {
        //ERROR
        return;
    }

    for (auto i = 0; i < actives; ++i) {
        struct kevent& ke = _data->events[i];
        if (ke.udata) {
            auto handler = static_cast<ScheduleHandler*>(ke.udata);
            (*handler)();
        }
    }
}

bool Loop::add_event(int fd, int events, void* handler) {
    struct timespec immediatelly;
    immediatelly.tv_nsec = 0;
    immediatelly.tv_sec = 0;
    struct kevent ev[2];
    int n = 0;
    if (events & POLLIN) {
        EV_SET(&ev[n++], fd, EVFILT_READ, EV_ADD|EV_ENABLE|EV_CLEAR, 0, 0, handler);
    }
    if (events & POLLOUT) {
        EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_ADD|EV_ENABLE|EV_CLEAR, 0, 0, handler);
    }

    return 0 == kevent(_backend_fd.fd(), ev, n, nullptr, 0, &immediatelly);
}

void Loop::remove_event(int fd, int events) {
    struct timespec immediatelly;
    immediatelly.tv_nsec = 0;
    immediatelly.tv_sec = 0;
    struct kevent ev[2];
    int n = 0;
    if (events & POLLIN) {
        EV_SET(&ev[n++], fd, EVFILT_READ, EV_DELETE|EV_DISABLE, 0, 0, nullptr);
    }
    if (events & POLLOUT) {
        EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_DELETE|EV_DISABLE, 0, 0, nullptr);
    }

    kevent(_backend_fd.fd(), ev, n, nullptr, 0, &immediatelly);
}

}
