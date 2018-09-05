// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/9.
//

#ifndef RPC_SCHEDULE_LOOP_THREAD_H
#define RPC_SCHEDULE_LOOP_THREAD_H

#include <atomic>
#include <thread>
#include <future>
#include <memory>
#include "tcp/socket_base.h"

namespace antflash {

class Loop;

class LoopThread final {
public:
    LoopThread();
    ~LoopThread();

    LoopThread(const LoopThread&) = delete;
    LoopThread&operator=(const LoopThread&) = delete;

    LoopThread(LoopThread&& right);
    LoopThread& operator=(LoopThread&& right);

    bool start();

    void stop();

    bool add_event(int fd, int events, void* handler);
    void remove_event(int fd, int events);

private:
    std::atomic<bool> _exit;
    std::unique_ptr<std::thread> _thread;
    std::unique_ptr<Loop> _loop;
    base::FdGuard _wakeup_fds[2];
};

}

#endif //RPC_SCHEDULE_LOOP_THREAD_H
