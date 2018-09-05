// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/16.
//

#ifndef RPC_COMMON_THREAD_POOL_H
#define RPC_COMMON_THREAD_POOL_H

#include <vector>
#include <thread>
#include <future>
#include <mutex>
#include <condition_variable>
#include <pthread.h>
#include <atomic>
#include "lockfree_queue.h"

namespace antflash {

template <typename F, typename En = void>
struct ThreadPoolTask {
    using ResultType = typename std::result_of<F()>::type;
    using SubmitReturnType = std::future<ResultType>;
    using Task = std::packaged_task<ResultType()>;

    [[gnu::always_inline]]
    static SubmitReturnType getResult(Task& task) {
        return task.get_future();
    }
};

template <typename F>
struct ThreadPoolTask<F, typename std::enable_if<
std::is_void<typename std::result_of<F()>::type>::value, void>::type> {
    using ResultType = void;
    using SubmitReturnType = bool;
    using Task = F;

    [[gnu::always_inline]]
    static SubmitReturnType getResult(Task&) {
        return true;
    }
};

template <typename F>
class ThreadPool {
public:
    using ResultType = typename ThreadPoolTask<F>::ResultType;
    using SubmitReturnType = typename ThreadPoolTask<F>::SubmitReturnType;
    using Task = typename ThreadPoolTask<F>::Task;
    using PackagedTask = Task*;

    ThreadPool(size_t capacity, size_t queue_size) :
        _active_td_size(0), 
        _td_capacity(capacity), 
        _queue_size(queue_size),
        _exit(false),
        _need_wakeup(0) {}

    ~ThreadPool() {
        _exit = true;
        {
            std::lock_guard<std::mutex> guard(_td_wakeup_mtx);
            _td_wakeup.notify_all();
        }
        for (auto& td : _tds) {
            if (td->td->joinable()) {
                td->td->join();
            }   

            auto status = td->status.load(std::memory_order_acquire);
            if (status == EThreadStatus::STANDBY) {
                delete td;
            } 
            //FIXME other status       
        }
    }

    bool init() {
        _tds.clear();
        _tds.reserve(_td_capacity);
        pthread_key_create(&_paired_thread, deleteThreadLocalPairThread);

        for (size_t i = 0; i < _td_capacity; ++i) {
            auto td = new SingleThread;
            td->td.reset(new std::thread([this, td, i](){
                td->id = std::this_thread::get_id();
                while (!_exit) {
                    auto status = td->status.load(std::memory_order_acquire);
                    if (status == EThreadStatus::ACTIVE) {
                        tryConsume(*td);
                    } else if (status == EThreadStatus::INACTIVE) {
                        tryConsume(*td);
                        td->status.store(EThreadStatus::STANDBY, std::memory_order_release);
                        _active_td_size.fetch_sub(1, std::memory_order_release);
                    }
                    if (!tryStealOne()) {
                        PackagedTask task = nullptr;
                        {
                            std::unique_lock<std::mutex> guard(_td_wakeup_mtx);
                            _need_wakeup.fetch_add(1, std::memory_order_relaxed);
                            _td_wakeup.wait(guard, [this, status, td, &task](){
                                if (!_exit) {
                                    return status == EThreadStatus::STANDBY ?
                                        tryGetOne(task) : tryGetOne(*td, task);
                                }
                                return true;
                            });
                            _need_wakeup.fetch_sub(1, std::memory_order_relaxed);
                        }
                        if (nullptr != task) {
                            (*task)();
                            delete task;
                        }
                    }
                }
                tryConsume(*td);
            }));
            _tds.emplace_back(td);
        }
        return true;
    }

    SubmitReturnType submit(F&& task) {
        auto task_package = new Task(std::forward<F>(task));
        auto ret = ThreadPoolTask<F>::getResult(*task_package);
        SingleThread* td = nullptr;
        if ((!getActiveThread(td)) || (!td->tasks->push(task_package))) {
            (*task_package)();
            delete task_package;
        }

        if (_need_wakeup.load(std::memory_order_relaxed) > 0) {
            std::unique_lock<std::mutex> guard(_td_wakeup_mtx);
            _td_wakeup.notify_all();
        }

        return ret;
    }

private:
    enum EThreadStatus : int32_t {
        ACTIVE,
        STANDBY,
        INACTIVE,
        RECLAIMED
    };

    struct SingleThread {
        std::unique_ptr<std::thread> td;
        std::thread::id id;
        std::atomic<EThreadStatus> status;
        std::unique_ptr<SPMCQueue<PackagedTask>> tasks;

        SingleThread() : status(EThreadStatus::STANDBY) {}
    };

    static void deleteThreadLocalPairThread(void* arg) {
        auto p_task = static_cast<SingleThread*>(arg);
        auto status = p_task->status.load(std::memory_order_acquire);
        do {
            if (status == EThreadStatus::RECLAIMED) {
                delete p_task;
                return;
            }
        } while (p_task->status.compare_exchange_strong(
                    status,
                    EThreadStatus::INACTIVE, 
                    std::memory_order_acq_rel, 
                    std::memory_order_relaxed));
    }

    bool getActiveThread(SingleThread*& td) {
        if (_exit) {
            return false;
        }

        if (_active_td_size.load(std::memory_order_relaxed) == _td_capacity) {
            return false;
        }

        auto thread = static_cast<SingleThread*>(
            pthread_getspecific(_paired_thread));
        
        if (nullptr == thread) {
            std::lock_guard<std::mutex> guard(_td_mtx);
            for (size_t i = 0; i < _td_capacity; ++i) {
                if (EThreadStatus::STANDBY == _tds[i]->status.load(std::memory_order_acquire)) {
                    thread = _tds[i];
                    if (!thread->tasks) {
                        thread->tasks.reset(new SPMCQueue<PackagedTask>(_queue_size));
                    }
                    thread->status.store(EThreadStatus::ACTIVE, std::memory_order_release);
                    _active_td_size.fetch_add(1, std::memory_order_release);
                    break;
                }
            }

            if (nullptr == thread) {
                return false;
            }
            pthread_setspecific(_paired_thread, thread);
        }

        td = thread;
        return true;
    }

    inline bool tryConsumeOne(SingleThread& td) {
        PackagedTask task = nullptr;
        if ((!_exit) && td.tasks->pop(task)) {
            (*task)();
            delete task;
            return true;
        }
        return false;
    }

    inline bool tryGetOne(SingleThread& td, PackagedTask& task) {
        if (td.tasks->pop(task)) {
            return true;
        }
        return false;
    }

    inline bool tryGetOne(PackagedTask& task) {
        size_t active_td = _active_td_size.load(std::memory_order_acquire);   
        //TODO 不要每个线程都从0开始，设定一个随机的初始偏移
        for (size_t i = 0; i < active_td; ++i) {
            auto& td = _tds[i];
            if (td->status.load(std::memory_order_acquire) != EThreadStatus::STANDBY) {
                if (tryGetOne(*td, task)) {
                    return true;
                }
            }
        }
        return false;
    }

    inline void tryConsume(SingleThread& td) {
        while (tryConsumeOne(td)) {
            ;
        } 
    }

    bool tryStealOne() {
        if (_exit) {
            return false;
        }
        size_t active_td = _active_td_size.load(std::memory_order_acquire);   
        //TODO 不要每个线程都从0开始，设定一个随机的初始偏移
        for (size_t i = 0; i < active_td; ++i) {
            auto& td = _tds[i];
            if (td->status.load(std::memory_order_acquire) != EThreadStatus::STANDBY) {
                if (tryConsumeOne(*td)) {
                    return true;
                }
            }
        }
        return false;
    }

    pthread_key_t _paired_thread;
    std::vector<SingleThread*> _tds;
    std::atomic<size_t> _active_td_size;
    size_t _td_capacity;
    size_t _queue_size;
    std::mutex _td_mtx;
    std::mutex _td_wakeup_mtx;
    std::condition_variable _td_wakeup;
    bool _exit;
    std::atomic<size_t> _need_wakeup;
};

}

#endif

