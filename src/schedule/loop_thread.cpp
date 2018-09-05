// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/9.
//

#include "loop_thread.h"
#include <functional>
#include <poll.h>
#include "loop.h"
#include "common/log.h"

namespace antflash {

LoopThread::LoopThread() : _exit(false) {
}

LoopThread::~LoopThread() {
}


LoopThread::LoopThread(LoopThread&& right) {
    _thread = std::move(right._thread);
    _exit.store(right._exit.load());
    right._exit.store(false);
}

LoopThread& LoopThread::operator=(LoopThread&& right) {
    if (&right != this) {
        _thread = std::move(right._thread);
        _exit.store(right._exit.load());
        right._exit.store(false);
    }

    return *this;
}

bool LoopThread::start() {
    int wakeup_fd[2];
    wakeup_fd[0] = -1;
    wakeup_fd[1] = -1;
    if (pipe(wakeup_fd) != 0) {
        return false;
    }
    _wakeup_fds[0] = wakeup_fd[0];
    _wakeup_fds[1] = wakeup_fd[1];

    std::promise<bool> thread_ok;
    _thread.reset(new std::thread([this, &thread_ok](){
        try {
            _loop.reset(new Loop());
            if (!_loop->init()) {
                thread_ok.set_value(false);
                return;
            }

            thread_ok.set_value(true);

            while (!_exit.load(std::memory_order_acquire)) {
                _loop->loop_once();
            }

            _loop->destroy();
        } catch (const std::exception& ex) {
            thread_ok.set_exception(std::make_exception_ptr(ex));
        }
    }));

    return thread_ok.get_future().get();
}

void LoopThread::stop() {
    //memory barrier
    _exit.store(true, std::memory_order_release);
    //wakeup loop
    add_event(_wakeup_fds[1].fd(), POLLOUT, nullptr);

    if (_thread && _thread->joinable()) {
        _thread->join();
    }
}

bool LoopThread::add_event(int fd, int events, void* handler) {
    return _loop->add_event(fd, events, handler);
}

void LoopThread::remove_event(int fd, int events) {
    _loop->remove_event(fd, events);
}

}
