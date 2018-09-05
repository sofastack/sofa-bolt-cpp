// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/16.
//

#include <common/life_cycle_lock.h>
#include <gtest/gtest.h>
#include <thread>

using namespace antflash;

TEST(LifCycleLockTest, singleThread) {
    LifeCycleLock lock;

    ASSERT_TRUE(lock.tryShared());
    ASSERT_EQ(lock.record(), 4);
    ASSERT_TRUE(lock.tryUpgrade());
    ASSERT_EQ(lock.record(), 6);
    ASSERT_FALSE(lock.tryExclusive());
    ASSERT_EQ(lock.record(), 6);
    lock.releaseShared();
    ASSERT_EQ(lock.record(), 2);
    ASSERT_TRUE(lock.tryUpgrade());
    ASSERT_EQ(lock.record(), 2);
    ASSERT_TRUE(lock.tryExclusive());
    ASSERT_EQ(lock.record(), 1);
    ASSERT_FALSE(lock.tryUpgrade());
    ASSERT_EQ(lock.record(), 3);
    ASSERT_FALSE(lock.tryShared());
    lock.releaseExclusive();
    ASSERT_EQ(lock.record(), 0);
    ASSERT_TRUE(lock.tryShared());
    ASSERT_EQ(lock.record(), 4);
    lock.releaseShared();
    ASSERT_EQ(lock.record(), 0);
}

TEST(LifCycleLockTest, multiThread) {
    LifeCycleLock lock;
    auto dest_time = std::chrono::system_clock::now()
                     + std::chrono::milliseconds(50);

    std::thread t1([&lock, &dest_time]() {
        std::this_thread::sleep_until(dest_time);
        LifeCycleShareGuard guard(lock);
        ASSERT_TRUE(guard.shared());
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    });
    std::thread t2([&lock, &dest_time]() {
        std::this_thread::sleep_until(dest_time);
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        {
            LifeCycleShareGuard guard(lock);
            ASSERT_FALSE(guard.shared());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_FALSE(lock.tryUpgrade());
        ASSERT_FALSE(lock.tryExclusive());
        lock.upgrade();
        lock.exclusive();
        lock.releaseExclusive();
    });

    std::thread t3([&lock, &dest_time]() {
        std::this_thread::sleep_until(dest_time);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_TRUE(lock.tryUpgrade());
        ASSERT_FALSE(lock.tryExclusive());
        lock.exclusive();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        lock.releaseExclusive();
    });

    t1.join();
    t2.join();
    t3.join();

    ASSERT_EQ(lock.record(), 0);
}
