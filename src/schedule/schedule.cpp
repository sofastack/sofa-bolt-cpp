// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/9.
//

#include "schedule.h"
#include "loop_thread.h"
#include "common/log.h"

namespace antflash {

Schedule::Schedule() {

}

Schedule::~Schedule() {
    destroy_schedule();
    destroy_time_schedule();
}

bool Schedule::init(int32_t schedule_num) {
    if (schedule_num <= 0) {
        schedule_num = std::thread::hardware_concurrency();
    }
    _threads.resize(schedule_num);
    bool ret = true;
    for (auto& thread : _threads) {
        if (!thread.start()) {
            ret = false;
            break;
        }
    }

    if (!ret) {
        destroy_schedule();
    }

    _time_thread.reset(new TimeThread);
    ret = _time_thread->init();
    if (!ret) {
        destroy_schedule();
        destroy_time_schedule();
    }

    return ret;
}

void Schedule::destroy_schedule() {
    for (auto& thread : _threads) {
        thread.stop();
    }
    _threads.clear();
}
void Schedule::destroy_time_schedule() {
    if (_time_thread) {
        _time_thread->destroy();
        _time_thread.reset();
    }
}

bool Schedule::addScheduleInternal(int fd, int events, void *handler, int idx) {
    if (_threads.size() == 0) {
        return false;
    }
    if (idx > 0) {
        return _threads[idx % _threads.size()].add_event(fd, events, handler);
    } else {
        return _threads[fd % _threads.size()].add_event(fd, events, handler);
    }
}

void Schedule::removeSchedule(int fd, int events, int idx) {
    if (_threads.size() == 0) {
        return;
    }
    if (idx > 0) {
        _threads[idx % _threads.size()].remove_event(fd, events);
    } else {
        _threads[fd % _threads.size()].remove_event(fd, events);
    }
}

size_t Schedule::scheduleThreadSize() const {
    return _threads.size();
}

}
