// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/25.
//

#include <gtest/gtest.h>
#include <thread>
#include "common/time_thread.h"
#include "common/utils.h"

using namespace antflash;

TEST(TimeThreadTest, init) {
    TimeThread time_thread;
    ASSERT_TRUE(time_thread.init());
    time_thread.destroy();
}

TEST(TimeThreadTest, schedule) {
    TimeThread time_thread;
    ASSERT_TRUE(time_thread.init());

    auto current = Utils::getHighPrecisionTimeStamp();
    ASSERT_GT(time_thread.schedule(10, [current](){
        auto now = Utils::getHighPrecisionTimeStamp();
        EXPECT_EQ((now - current) / 1000, 10UL);
    }), 0U);

    std::this_thread::sleep_for(std::chrono::milliseconds(11));
}

TEST(TimeThreadTest, multi_schedule) {
    TimeThread time_thread;
    ASSERT_TRUE(time_thread.init());

    std::thread td1([&time_thread]{
        auto current = Utils::getHighPrecisionTimeStamp();
        ASSERT_GT(time_thread.schedule(10, [current](){
            auto now = Utils::getHighPrecisionTimeStamp();
            ASSERT_EQ((now - current) / 1000, 10UL);
        }), 0U);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(11));
    });

    std::thread td2([&time_thread]{
        auto current = Utils::getHighPrecisionTimeStamp();
        ASSERT_GT(time_thread.schedule(10, [current](){
            auto now = Utils::getHighPrecisionTimeStamp();
            ASSERT_EQ((now - current) / 1000, 10UL);
        }), 0U);

        std::this_thread::sleep_for(std::chrono::milliseconds(11));
    });

    std::thread td3([&time_thread]{
        auto current = Utils::getHighPrecisionTimeStamp();
        ASSERT_GT(time_thread.schedule(11, [current](){
            auto now = Utils::getHighPrecisionTimeStamp();
            ASSERT_EQ((now - current) / 1000, 11UL);
        }), 0U);

        std::this_thread::sleep_for(std::chrono::milliseconds(12));
    });

    td1.join();
    td2.join();
    td3.join();
}

TEST(TimeThreadTest, unschedule) {
    TimeThread time_thread;
    ASSERT_TRUE(time_thread.init());

    bool executed = false;
    auto task_id = time_thread.schedule(10, [&executed](){
        executed = true;
    });
    ASSERT_GT(task_id, 0UL);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    ASSERT_TRUE(time_thread.unschedule(task_id));
    std::this_thread::sleep_for(std::chrono::milliseconds(6));
    ASSERT_TRUE(!executed);

    executed = false;
    auto old_task_id = task_id;
    task_id = time_thread.schedule(10, [&executed](){
        executed = true;
    });
    ASSERT_GT(task_id, old_task_id);
    std::this_thread::sleep_for(std::chrono::microseconds(9800));
    ASSERT_TRUE(time_thread.unschedule(task_id));
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    ASSERT_TRUE(!executed);
}

TEST(TimeThreadTest, multi_unschedule) {
    TimeThread time_thread;
    ASSERT_TRUE(time_thread.init());

    std::thread td1([&time_thread]{
        bool executed = false;
        auto task_id = time_thread.schedule(10, [&executed](){
            executed = true;
        });
        ASSERT_GT(task_id, 0UL);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        ASSERT_TRUE(time_thread.unschedule(task_id));
        std::this_thread::sleep_for(std::chrono::milliseconds(6));
        ASSERT_TRUE(!executed);
    });

    std::thread td2([&time_thread]{
        bool executed = false;
        auto task_id = time_thread.schedule(10, [&executed](){
            executed = true;
        });
        ASSERT_GT(task_id, 0UL);
        std::this_thread::sleep_for(std::chrono::microseconds(9800));
        ASSERT_TRUE(time_thread.unschedule(task_id));
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        ASSERT_TRUE(!executed);
    });

    std::thread td3([&time_thread]{
        bool executed = false;
        auto task_id = time_thread.schedule(10, [&executed](){
            executed = true;
        });
        ASSERT_GT(task_id, 0UL);
        std::this_thread::sleep_for(std::chrono::milliseconds(12));
        ASSERT_TRUE(time_thread.unschedule(task_id));
        ASSERT_TRUE(executed);
    });

    td1.join();
    td2.join();
    td3.join();
}

TEST(TimeThreadTest, multi_cross_unschedule) {
    TimeThread time_thread;
    ASSERT_TRUE(time_thread.init());

    size_t task_id = 0;
    std::thread td1([&time_thread, &task_id]{
        bool executed = false;
        task_id = time_thread.schedule(10, [&executed](){
            executed = true;
        });
        ASSERT_GT(task_id, 0UL);
        std::this_thread::sleep_for(std::chrono::milliseconds(11));
        ASSERT_TRUE(!executed);
    });

    std::thread td2([&time_thread, &task_id]{
        while (task_id == 0UL) {
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        ASSERT_TRUE(time_thread.unschedule(task_id));
    });

    std::thread td3([&time_thread, &task_id]{
        while (task_id == 0UL) {
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(12));
        ASSERT_TRUE(time_thread.unschedule(task_id));
    });

    td1.join();
    td2.join();
    td3.join();
}


