// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/5/9.
//

#include "common/time_thread.h"
#include <algorithm>
#include <future>
#include "common/utils.h"
#include "common/lockfree_queue.h"
#include "common/macro.h"
#include "common/log.h"

namespace antflash {

struct Task {
    size_t abs_time;
    size_t task_id;
    TimerTaskFn fn;

    Task() : abs_time(std::numeric_limits<size_t>::max()), task_id(0){}
    Task(size_t at, size_t id, TimerTaskFn&& f) :
        abs_time(at), task_id(id), fn(std::forward<TimerTaskFn>(f)) {}
    Task(Task&&) = default;
    Task& operator=(Task&&) = default;

    inline bool operator<(const Task& right) const {
        return abs_time > right.abs_time;
    }
};

struct AbandonTask {
    size_t abs_time;
    size_t task_id;

    AbandonTask() = default;
    ~AbandonTask() = default;
    AbandonTask(size_t at, size_t id) :
        abs_time(at), task_id(id) {}
    AbandonTask(AbandonTask&&) = default;
    AbandonTask& operator=(AbandonTask&&) = default;
};

struct TaskContainer {
    enum EStatus : int32_t {
        AVAILABLE = 0,
        USING,
        RECLAIMED
    };

    TaskContainer(): status(AVAILABLE), 
                     tasks(TASK_QUEUE_SIZE), 
                     abandon_tasks(TASK_QUEUE_SIZE) {
    }

    std::atomic<EStatus> status;
    SPSCQueue<Task> tasks;
    SPSCQueue<AbandonTask> abandon_tasks;
    static constexpr size_t  TASK_QUEUE_SIZE = 4096;
    static constexpr size_t  WARING_TASK_QUEUE_SIZE =
                (size_t)((double)TASK_QUEUE_SIZE * 2.0 / 3.0);
};

TimeThread::TimeThread() : 
    _exit(false),
    _nearest_run_time(std::numeric_limits<size_t>::max()),
    _timer_id(1),
    _tasks(new std::vector<Task>()) {

}

TimeThread::~TimeThread() {
    destroy();
}

static void deleteThreadLocalTaskContainer(void* arg) {
    //FIXME
    //auto p_task = static_cast<TaskContainer*>(arg);
    //p_task->status.store(TaskContainer::RECLAIMED, std::memory_order_release);
}

bool TimeThread::init() {
    _exit.store(false, std::memory_order_release);

    pthread_key_create(&_local_task, deleteThreadLocalTaskContainer);
    _containers.reserve(std::thread::hardware_concurrency());
    std::promise<bool> init_ret;
    _td.reset(new std::thread([this, &init_ret](){
        init_ret.set_value(true);
        while (!_exit.load(std::memory_order_acquire)) {
            loopOnce();
        }
    }));

    return init_ret.get_future().get();
}

void TimeThread::destroy() {
    if (!_exit.load(std::memory_order_acquire)) {
        _exit.store(true, std::memory_order_release);
        {
            std::lock_guard<std::mutex> guard(_wake_mtx);
            _wake_cond.notify_one();
        }
        if (_td && _td->joinable()) {
            _td->join();
            _td.reset();
        }

        for (auto &container : _containers) {
            //wait until queue is empty
            std::lock_guard<std::mutex> guard(_tasks_mtx);
            Task task;
            while (container->tasks.pop(task)) {
                _active_ids.emplace(task.task_id);
                _tasks->emplace_back(std::move(task));
                std::push_heap(_tasks->begin(), _tasks->end());
            }
            AbandonTask abandon_task;
            while (container->abandon_tasks.pop(abandon_task)) {
                auto itr = _active_ids.find(abandon_task.task_id);
                if (itr != _active_ids.end()) {
                    _active_ids.erase(itr);
                }
            }

            delete container;
            container = nullptr;
        }

        for (auto& task : *_tasks) {
            auto itr = _active_ids.find(task.task_id);
            if (itr != _active_ids.end()) {
                LOG_DEBUG("destroy and deal with task:{}", task.task_id);
                task.fn();
                _active_ids.erase(itr);
            }
        }
    }
}

size_t TimeThread::schedule(size_t timeout, TimerTaskFn fn) {
    size_t abs_time = Utils::getHighPrecisionTimeStamp() + timeout * 1000;
    return scheduleAbs(abs_time, std::move(fn));
}

size_t TimeThread::scheduleAbs(size_t abs_time, TimerTaskFn&& fn) {
    if (_exit.load(std::memory_order_acquire)) {
        return 0;
    }

    auto local_task = getLocalTaskContainer();
    if (nullptr == local_task) {
        return 0;
    }

    size_t task_id = _timer_id.fetch_add(1, std::memory_order_relaxed);
    auto& tasks = local_task->tasks;
    if (UNLIKELY(!tasks.push(abs_time, task_id, std::move(fn)))) {
        return 0;
    }
    
    size_t nearest_time = _nearest_run_time.load(std::memory_order_acquire);
    while (true) {
        if (abs_time >= nearest_time) {
            break;
        }
        if(_nearest_run_time.compare_exchange_strong(
                nearest_time, abs_time, 
                std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
            std::lock_guard<std::mutex> guard(_wake_mtx);
            _wake_cond.notify_one();
            break;
        }
    } 
    
    if (tasks.size() >= TaskContainer::WARING_TASK_QUEUE_SIZE) {
        std::lock_guard<std::mutex> guard(_wake_mtx);
        _wake_cond.notify_one();
    }

    return task_id;
}

bool TimeThread::unschedule(size_t task_id) {
    auto local_task = getLocalTaskContainer();
    auto& tasks = local_task->abandon_tasks;
    size_t abs_time = Utils::getHighPrecisionTimeStamp();
    if (UNLIKELY(!tasks.push(abs_time, task_id))) {
        return false;
    }

    if (tasks.size() >= TaskContainer::WARING_TASK_QUEUE_SIZE) {
        _wake_cond.notify_one();
    }

    return true;
}

void TimeThread::collectUnschedule(TaskContainer* container) {
    size_t cur_size = container->abandon_tasks.size();
    AbandonTask abandon_task;
    for (size_t i = 0; i < cur_size; ++i) {
        if (UNLIKELY(!container->abandon_tasks.pop(abandon_task))) {
            break;
        }
        //If taskid exist and flag is ture, it means this taskid is still active
        auto itr = _active_ids.find(abandon_task.task_id);
        if (itr != _active_ids.end()) {
            _active_ids.erase(itr);
        }
    }
}

void TimeThread::collectUnschedule() {
    //Higher priority
    std::lock_guard<std::mutex> guard(_tasks_mtx);
    size_t task_container_size = _containers.size();

    for (size_t i = 0; i < task_container_size; ++i) {
        auto container = _containers[i];
        if (container->status == TaskContainer::AVAILABLE) {
            continue;
        }
        collectUnschedule(container);   
    }
}

void TimeThread::loopOnce() {
    Utils::Timer timer;
    size_t nearest_run_time = _nearest_run_time.load(std::memory_order_acquire);
    if (!_nearest_run_time.compare_exchange_strong(
                nearest_run_time, 
                std::numeric_limits<size_t>::max(),
                std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
        //Other earlier schedule is pushed to task container, return and loop again
        return;
    }

    size_t task_container_size = 0;
    {
        std::lock_guard<std::mutex> guard(_tasks_mtx);
        task_container_size = _containers.size();
    }
    _active_ids.reserve(task_container_size * TaskContainer::TASK_QUEUE_SIZE);
    _tasks->reserve(task_container_size * TaskContainer::TASK_QUEUE_SIZE);
    for (size_t i = 0; i < task_container_size; ++i) {
        TaskContainer* container = nullptr;
        {
            std::lock_guard<std::mutex> guard(_tasks_mtx);
            container = _containers[i];
        }
        if (container->status == TaskContainer::AVAILABLE) {
            continue;
        }
        
        Task task;
        size_t task_size = container->tasks.size();
        for (size_t i = 0; i < task_size; ++i) {
            if (UNLIKELY(!container->tasks.pop(task))) {
                break;
            }
            _active_ids.emplace(task.task_id);
            _tasks->emplace_back(std::move(task));
            std::push_heap(_tasks->begin(), _tasks->end());
        }

        collectUnschedule(container);
    }

    while (!_tasks->empty()) {
        size_t abs_time = Utils::getHighPrecisionTimeStamp();
        auto& task = _tasks->at(0);
        if (task.abs_time > abs_time) {
            break;
        }

        nearest_run_time = _nearest_run_time.load(std::memory_order_acquire);
        //Other earlier schedule is pushed to task container, return and loop again
        if (task.abs_time > nearest_run_time) {
            return;
        }

        std::pop_heap(_tasks->begin(), _tasks->end());
        auto this_task = std::move(_tasks->back());
        _tasks->pop_back();
        
        //try collect unschedule event again
        collectUnschedule();

        auto itr = _active_ids.find(this_task.task_id);
        if (itr != _active_ids.end()) {
            this_task.fn();
            _active_ids.erase(itr);
        }
    }

    size_t next_near_time = std::numeric_limits<size_t>::max(); 
    if (!_tasks->empty()) {
        next_near_time = _tasks->at(0).abs_time;
    }

    nearest_run_time = _nearest_run_time.load(std::memory_order_acquire);
    while (nearest_run_time > next_near_time) {
        if (_nearest_run_time.compare_exchange_strong(
                nearest_run_time, next_near_time,
                std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
            nearest_run_time = next_near_time;
        }
    }

    auto cur_time = Utils::getHighPrecisionTimeStamp();
    if (nearest_run_time <= cur_time) {
        return;
    }
    auto next_run_time = nearest_run_time - cur_time;
    constexpr size_t max_next_run_time = (size_t)3600 * 1000 * 1000;
    if (next_run_time > max_next_run_time) {
        next_run_time = max_next_run_time;
    }

    std::unique_lock<std::mutex> guard(_wake_mtx);
    _wake_cond.wait_for(guard, std::chrono::microseconds(next_run_time),
                        [nearest_run_time, this]() {
                            return _exit.load(std::memory_order_acquire) 
                            || nearest_run_time != 
                                _nearest_run_time.load(std::memory_order_acquire);
                        });
}

TaskContainer* TimeThread::getLocalTaskContainer() {
    if (_exit.load(std::memory_order_acquire)) {
        return nullptr;
    }

    TaskContainer* task = static_cast<TaskContainer*>(
            pthread_getspecific(_local_task));
    if (nullptr == task) {
        std::lock_guard<std::mutex> guard(_tasks_mtx);

        for (auto& t : _containers) {
            if (t->status.load(std::memory_order_acquire)
                    == TaskContainer::AVAILABLE) {
                task = t;
                break;
            }
        }
        if (nullptr == task) {
            task = new TaskContainer;
            _containers.emplace_back(task);
        }
        task->status = TaskContainer::USING;
        pthread_setspecific(_local_task, task);
    }

    return task;
}

}

