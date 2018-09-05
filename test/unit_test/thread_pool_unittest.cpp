// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/25.
//

#include "common/thread_pool.h"
#include <gtest/gtest.h>
#include <thread>
#include <assert.h>
#include "common/utils.h"

using namespace antflash;

TEST(ThreadPoolTest, init) {
    using Task = std::function<int()>;
    ThreadPool<Task> thread_pool(2, 128);
    ASSERT_TRUE(thread_pool.init());
}

TEST(ThreadPoolTest, singleTask) {
    using Task = std::function<int()>;
    ThreadPool<Task> thread_pool(2, 8);
    ASSERT_TRUE(thread_pool.init());
    auto call_td_id = std::this_thread::get_id();
    auto ret = thread_pool.submit([&call_td_id]() -> int {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        auto run_td_id = std::this_thread::get_id();
        if (run_td_id == call_td_id) {
            return 0;
        }
        return 1;
    });
    
    ASSERT_EQ(ret.get(), 1);
}

TEST(ThreadPoolTest, singleVoidTask) {
    using Task = std::function<void()>;
    ThreadPool<Task> thread_pool(2, 8);
    ASSERT_TRUE(thread_pool.init());
    auto run_td_id = std::this_thread::get_id();
    auto ret = thread_pool.submit([&run_td_id]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        run_td_id = std::this_thread::get_id();
    });
    ASSERT_TRUE(ret);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_NE(run_td_id, std::this_thread::get_id());
}

TEST(ThreadPoolTest, singleTaskNoPool) {
    using Task = std::function<int()>;
    ThreadPool<Task> thread_pool(0, 0);
    ASSERT_TRUE(thread_pool.init());
    auto call_td_id = std::this_thread::get_id();
    auto ret = thread_pool.submit([&call_td_id]() -> int {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        auto run_td_id = std::this_thread::get_id();
        if (run_td_id == call_td_id) {
            return 0;
        }
        return 1;
    });
    
    ASSERT_EQ(ret.get(), 0);
}

TEST(ThreadPoolTest, twoTaskParallel) {
    using Task = std::function<int()>;
    ThreadPool<Task> thread_pool(2, 4);
    ASSERT_TRUE(thread_pool.init());

    auto dest_time = std::chrono::system_clock::now()
                    + std::chrono::milliseconds(50);

    auto td1_pool_id = std::this_thread::get_id();
    std::thread td1([&dest_time, &thread_pool, &td1_pool_id]() {
        std::this_thread::sleep_until(dest_time);
        auto call_td_id = std::this_thread::get_id();
        auto ret = thread_pool.submit([&call_td_id, &td1_pool_id]() -> int {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            td1_pool_id = std::this_thread::get_id();
            if (td1_pool_id == call_td_id) {
                return 0;
            }
            return 1;
        });
    
        ASSERT_EQ(ret.get(), 1);
    });
    auto td2_pool_id = std::this_thread::get_id();
    std::thread td2([&dest_time, &thread_pool, &td2_pool_id]() {
        std::this_thread::sleep_until(dest_time);
        auto call_td_id = std::this_thread::get_id();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto ret = thread_pool.submit([&call_td_id, &td2_pool_id]() -> int {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            td2_pool_id = std::this_thread::get_id();
            if (td2_pool_id == call_td_id) {
                return 0;
            }
            return 1;
        });
    
        ASSERT_EQ(ret.get(), 1);
    });
    td1.join();
    td2.join();
    ASSERT_NE(td1_pool_id, td2_pool_id);
}

TEST(ThreadPoolTest, taskSteal) {
    using Task = std::function<int()>;
    ThreadPool<Task> thread_pool(2, 4);
    ASSERT_TRUE(thread_pool.init());

    auto td1_pool_id = std::this_thread::get_id();
    auto td2_pool_id = std::this_thread::get_id();
    std::thread td1([&thread_pool, &td1_pool_id, &td2_pool_id]() {
        auto call_td_id = std::this_thread::get_id();
        auto ret1 = thread_pool.submit([&call_td_id, &td1_pool_id]() -> int {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            td1_pool_id = std::this_thread::get_id();
            if (td1_pool_id == call_td_id) {
                return 0;
            }
            return 1;
        });

        auto ret2 = thread_pool.submit([&call_td_id, &td2_pool_id]() -> int {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            td2_pool_id = std::this_thread::get_id();
            if (td2_pool_id == call_td_id) {
                return 0;
            }
            return 1;
        });
    
        ASSERT_EQ(ret1.get(), 1);
        ASSERT_EQ(ret2.get(), 1);
    });
    td1.join();
    ASSERT_NE(td1_pool_id, td2_pool_id);
}

TEST(ThreadPoolTest, taskPerformance) {
    using Task = std::function<size_t()>;
    constexpr size_t task_num = 100000;
    {
        ThreadPool<Task> thread_pool(2, 1024);
        ASSERT_TRUE(thread_pool.init());
        std::vector<std::future<size_t>> rets;
        rets.reserve(task_num);
        bool* results = new bool[task_num];
        Utils::Timer cost;
        for (size_t i = 0; i < task_num; ++i) {
            results[i] = false;
            rets.emplace_back(thread_pool.submit([&results, i]()-> size_t {
                        results[i] = true;
                        return i;}));
        }
        for (auto& ret : rets) {
            ASSERT_GE(ret.get(), 0UL);
        }
        std::cout << "time cost:" << cost.elapsed() << "ms." << std::endl;
        for (size_t i = 0; i < task_num; ++i) {
            ASSERT_TRUE(results[i]);
        }
        delete [] results;
    }
    {
        ThreadPool<Task> thread_pool(4, 1024);
        ASSERT_TRUE(thread_pool.init());
        std::vector<std::future<size_t>> rets;
        rets.reserve(task_num);
        bool* results = new bool[task_num];
        Utils::Timer cost;
        for (size_t i = 0; i < task_num; ++i) {
            results[i] = false;
            rets.emplace_back(thread_pool.submit([&results, i]()-> size_t {
                        results[i] = true;
                        return i;}));
        }
        for (auto& ret : rets) {
            ASSERT_GE(ret.get(), 0UL);
        }
        std::cout << "time cost:" << cost.elapsed() << "ms." << std::endl;
        for (size_t i = 0; i < task_num; ++i) {
            ASSERT_TRUE(results[i]);
        }
        delete [] results;
    }
    {
        using VoidTask = std::function<void()>;
        ThreadPool<VoidTask> thread_pool(2, 1024);
        ASSERT_TRUE(thread_pool.init());
        bool* results = new bool[task_num];
        std::atomic<size_t> counter(0);
        Utils::Timer cost;
        for (size_t i = 0; i < task_num; ++i) {
            results[i] = false;
            thread_pool.submit([&counter, &results, i]()-> void {
                        results[i] = true;
                        counter.fetch_add(1);
            });
        }
        while (counter.load() < task_num) {
            ;
        }
        std::cout << "time cost:" << cost.elapsed() << "ms." << std::endl;
        for (size_t i = 0; i < task_num; ++i) {
            ASSERT_TRUE(results[i]);
        }

        delete [] results;
    }
}
