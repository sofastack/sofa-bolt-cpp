// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/9.
//

#ifndef RPC_INCLUDE_SCHEDULE_H
#define RPC_INCLUDE_SCHEDULE_H

#include <vector>
#include <future>
#include <functional>
#include <memory>
#include "common/time_thread.h"

namespace antflash {

class LoopThread;

class Schedule {
public:
    using Handler = std::function<void()>;

    //Singleton
    static Schedule& getInstance() {
        static Schedule schedule;
        return schedule;
    }

    bool init(int32_t schedule_num = -1);
    void destroy_schedule();
    void destroy_time_schedule();

    //TODO change fd + handler to a class
    bool addSchedule(int fd, int events, int idx = -1) {
        return addScheduleInternal(fd, events, nullptr, idx);
    }

    bool addSchedule(int fd, int events, Handler &handler, int idx = -1) {
        return addScheduleInternal(fd, events, (void*)&handler, idx);
    }

    void removeSchedule(int fd, int events, int idx = -1);

    size_t addTimeschdule(size_t abs_time, TimerTaskFn&& fn) {
        return _time_thread->scheduleAbs(abs_time, std::move(fn));
    }

    bool removeTimeschdule(size_t time_task_id) {
        return _time_thread->unschedule(time_task_id);
    }

    size_t scheduleThreadSize() const;

private:
    bool addScheduleInternal(int fd, int events, void *handler, int idx);

    Schedule();
    ~Schedule();

    std::vector<LoopThread> _threads;
    std::unique_ptr<TimeThread> _time_thread;
};



}

#endif //RPC_INCLUDE_SCHEDULE_H
