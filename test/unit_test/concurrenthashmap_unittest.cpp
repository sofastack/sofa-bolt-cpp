// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/5/22.
//

#include "common/concurrent_hash_map.h"
#include <string>
#include <gtest/gtest.h>
#include <thread>
#include <random>
#include "common/utils.h"

constexpr size_t s_bucket_size = 1024;

TEST(ConcurrentHashMapTest, singleThread) {
    antflash::ConcurrentHashMap<size_t, size_t>
            hash_map(s_bucket_size);

    ASSERT_EQ(hash_map.bucketSize(), 1031UL);
    ASSERT_EQ(hash_map.size(), 0UL);

    ASSERT_EQ(hash_map.getHash(0) % hash_map.bucketSize(),
              hash_map.getHash(hash_map.bucketSize()) % hash_map.bucketSize());

    antflash::ConcurrentHashMap<std::string, size_t>
            str_hash_map(s_bucket_size, s_bucket_size);

    ASSERT_EQ(str_hash_map.bucketSize(), 1031UL);
    ASSERT_EQ(str_hash_map.size(), 0UL);

    ASSERT_NE(str_hash_map.getHash("") % hash_map.bucketSize(),
              str_hash_map.getHash("abc") % hash_map.bucketSize());

    size_t ret = 0;
    ASSERT_FALSE(hash_map.get(1, ret));
    ASSERT_TRUE(hash_map.put(1, 10));
    ASSERT_TRUE(hash_map.get(1, ret));
    ASSERT_FALSE(hash_map.get(1 + hash_map.bucketSize(), ret));
    ASSERT_EQ(ret, 10UL);
    ASSERT_TRUE(hash_map.put(1 + hash_map.bucketSize(), 101));
    ASSERT_TRUE(hash_map.get(1 + hash_map.bucketSize(), ret));
    ASSERT_EQ(ret, 101UL);

    ret = 11;
    ASSERT_FALSE(hash_map.put(1, ret));
    ASSERT_TRUE(hash_map.exchange(1, ret));
    ASSERT_EQ(ret, 10UL);
    ASSERT_TRUE(hash_map.get(1, ret));
    ASSERT_EQ(ret, 11UL);

    ASSERT_FALSE(hash_map.put(1 + hash_map.bucketSize(), ret));
    ASSERT_TRUE(hash_map.exchange(1 + hash_map.bucketSize(), ret));
    ASSERT_EQ(ret, 101UL);
    ASSERT_TRUE(hash_map.get(1 + hash_map.bucketSize(), ret));
    ASSERT_EQ(ret, 11UL);
    hash_map.erase(1 + hash_map.bucketSize());
    ASSERT_FALSE(hash_map.get(1 + hash_map.bucketSize(), ret));

    ASSERT_FALSE(hash_map.exchange(2, ret));
    ASSERT_EQ(ret, 11UL);
    ret = 9;
    ASSERT_TRUE(hash_map.get(2, ret));
    ASSERT_EQ(ret, 11UL);
    ASSERT_TRUE(hash_map.erase(2));
    ASSERT_FALSE(hash_map.erase(2));

    ret = 0;
    ASSERT_FALSE(str_hash_map.get("1", ret));
    ASSERT_TRUE(str_hash_map.put("1", 10));
    ASSERT_TRUE(str_hash_map.get("1", ret));
    ASSERT_EQ(ret, 10UL);

    ret = 11;
    ASSERT_FALSE(str_hash_map.put("1", ret));
    ASSERT_TRUE(str_hash_map.exchange("1", ret));
    ASSERT_EQ(ret, 10UL);
    ASSERT_TRUE(str_hash_map.get("1", ret));
    ASSERT_EQ(ret, 11UL);

    ASSERT_FALSE(str_hash_map.exchange("123", ret));
    ASSERT_EQ(ret, 11UL);
    ret = 9;
    ASSERT_TRUE(str_hash_map.get("123", ret));
    ASSERT_EQ(ret, 11UL);
}

TEST(ConcurrentHashMapTest, multiThread) {

    constexpr uint32_t thread_num = 4;
    constexpr uint32_t num_inserts = 1000000 / thread_num;
    constexpr uint32_t num_ops_perThread = num_inserts;

    antflash::ConcurrentHashMap<uint32_t, uint32_t>
            hash_map(num_inserts, s_bucket_size);

    antflash::Utils::Timer cost;

    std::vector<std::thread> threads;
    for (int64_t j = 0; j < thread_num; j++) {
        std::random_device rd;
        threads.emplace_back([&hash_map, j]() {
            for (uint32_t i = 0; i < num_ops_perThread; ++i) {
                hash_map.put(i, i / 3);
            }
        });
    }

    for (auto& td : threads) {
        td.join();
    }

    printf("set time cost: %lu us\n", cost.elapsedMicro());
    ASSERT_EQ(hash_map.size(), num_inserts);
    for (uint32_t i = 0; i < num_inserts; ++i) {
        uint32_t val = 0;
        EXPECT_TRUE(hash_map.get(i, val));
        EXPECT_EQ(val, i / 3);
    }

    threads.clear();
    cost.reset();
    for (int64_t j = 0; j < thread_num; j++) {
        std::random_device rd;
        threads.emplace_back([&hash_map, j]() {
            for (uint32_t i = 0; i < num_ops_perThread; ++i) {
                uint32_t val = 0;
                hash_map.get(0, val);
            }
        });
    }
    printf("find time cost: %lu us\n", cost.elapsedMicro());
    for (auto& td : threads) {
        td.join();
    }
}
