// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/5/9.
//


#ifndef RPC_SCHEDULE_TIME_THREAD_H
#define RPC_SCHEDULE_TIME_THREAD_H

#include <thread>
#include <memory>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <pthread.h>

namespace antflash {

struct Task;
struct TaskContainer;

using TimerTaskFn = std::function<void()>;

class TimeThread {
public:
    TimeThread();
    ~TimeThread();
    bool init();
    void destroy();
    size_t schedule(size_t timeout, TimerTaskFn fn);
    size_t scheduleAbs(size_t abs_time, TimerTaskFn&& fn);
    bool unschedule(size_t task_id);
private:
    void loopOnce();
    TaskContainer* getLocalTaskContainer();
    void collectUnschedule(TaskContainer*);
    void collectUnschedule();

    std::unique_ptr<std::thread> _td;
    std::atomic<bool> _exit;
    std::mutex _wake_mtx;
    std::condition_variable _wake_cond;

    std::mutex _tasks_mtx;
    std::vector<TaskContainer*> _containers;
    pthread_key_t _local_task;

    std::atomic<size_t> _nearest_run_time;
    std::atomic<size_t> _timer_id;

    std::unordered_set<size_t> _active_ids;
    std::unique_ptr<std::vector<Task>> _tasks;
};

}

#endif //RPC_SCHEDULE_TIME_THREAD_H
