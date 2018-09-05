// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/7/3.
//

#include "common/lru_cache.h"
#include <string>
#include <gtest/gtest.h>
#include <thread>
#include <random>
#include "common/utils.h"

constexpr size_t s_bucket_size = 1024;

TEST(LRUCacheTest, singleThread) {
    size_t max_mem_size = 24;
    antflash::LRUCache<size_t, size_t> cache(s_bucket_size, max_mem_size);
    ASSERT_EQ(cache.get_hit_query_times(), 0UL);
    ASSERT_EQ(cache.get_total_query_times(), 0UL);
    ASSERT_EQ(cache.get_mem_size_used(), 0UL);
    ASSERT_EQ(cache.size(), 0UL);

    size_t total_get = 0;
    size_t success_get = 0;
    size_t element = 0;
    size_t val = 0L;

    ASSERT_FALSE(cache.get(1L, val));
    ASSERT_EQ(cache.get_total_query_times(), ++total_get);
    ASSERT_EQ(cache.get_hit_query_times(), success_get);

    ASSERT_TRUE(cache.put(1L, 8L, 10L));
    ASSERT_TRUE(cache.get(1L, val));
    ASSERT_EQ(val, 10UL);
    ASSERT_EQ(cache.get_total_query_times(), ++total_get);
    ASSERT_EQ(cache.get_hit_query_times(), ++success_get);
    ASSERT_EQ(cache.get_mem_size_used(), 8UL);
    ASSERT_EQ(cache.size(), ++element);

    ASSERT_FALSE(cache.put(1L, 8L, 12L));
    ASSERT_EQ(cache.get_mem_size_used(), 8UL);


    ASSERT_TRUE(cache.exchange(1L, 8L, 12L));
    ASSERT_EQ(cache.get_mem_size_used(), 8UL);
    ASSERT_EQ(cache.size(), element);

    ASSERT_TRUE(cache.get(1L, val));
    ASSERT_EQ(val, 12UL);
    ASSERT_EQ(cache.get_total_query_times(), ++total_get);
    ASSERT_EQ(cache.get_hit_query_times(), ++success_get);

    ASSERT_TRUE(cache.put(2L, 8L, 13L));
    ASSERT_EQ(cache.get_mem_size_used(), 16UL);
    ASSERT_EQ(cache.size(), ++element);

    ASSERT_TRUE(cache.get(2L, val));
    ASSERT_EQ(val, 13UL);
    ASSERT_EQ(cache.get_total_query_times(), ++total_get);
    ASSERT_EQ(cache.get_hit_query_times(), ++success_get);

    ASSERT_TRUE(cache.get(1L, val));
    ASSERT_EQ(val, 12UL);
    ASSERT_EQ(cache.get_total_query_times(), ++total_get);
    ASSERT_EQ(cache.get_hit_query_times(), ++success_get);

    ASSERT_FALSE(cache.get(3L, val));
    ASSERT_EQ(val, 12UL);
    ASSERT_EQ(cache.get_total_query_times(), ++total_get);
    ASSERT_EQ(cache.get_hit_query_times(), success_get);

    ASSERT_FALSE(cache.exchange(3L, 8L, 14L));
    ASSERT_EQ(cache.get_mem_size_used(), 24UL);
    ASSERT_EQ(cache.size(), ++element);

    ASSERT_TRUE(cache.get(3L, val));
    ASSERT_EQ(val, 14UL);
    ASSERT_EQ(cache.get_total_query_times(), ++total_get);
    ASSERT_EQ(cache.get_hit_query_times(), ++success_get);

    ASSERT_TRUE(cache.put(4L, 8L, 15L));
    ASSERT_EQ(cache.get_mem_size_used(), 24UL);
    ASSERT_EQ(cache.size(), element);

    ASSERT_TRUE(cache.get(4L, val));
    ASSERT_EQ(val, 15UL);
    ASSERT_EQ(cache.get_total_query_times(), ++total_get);
    ASSERT_EQ(cache.get_hit_query_times(), ++success_get);

    ASSERT_FALSE(cache.get(2L, val));
    ASSERT_EQ(cache.get_total_query_times(), ++total_get);
    ASSERT_EQ(cache.get_hit_query_times(), success_get);

    ASSERT_TRUE(cache.put(5L, 8L, 16L));
    ASSERT_EQ(cache.get_mem_size_used(), 24UL);
    ASSERT_EQ(cache.size(), element);
    ASSERT_FALSE(cache.exchange(6L, 8L, 17L));
    ASSERT_EQ(cache.get_mem_size_used(), 24UL);
    ASSERT_EQ(cache.size(), element);

    ASSERT_FALSE(cache.get(1L, val));
    ASSERT_EQ(cache.get_total_query_times(), ++total_get);
    ASSERT_EQ(cache.get_hit_query_times(), success_get);

    ASSERT_FALSE(cache.get(2L, val));
    ASSERT_EQ(cache.get_total_query_times(), ++total_get);
    ASSERT_EQ(cache.get_hit_query_times(), success_get);

    ASSERT_FALSE(cache.get(3L, val));
    ASSERT_EQ(cache.get_total_query_times(), ++total_get);
    ASSERT_EQ(cache.get_hit_query_times(), success_get);

    ASSERT_TRUE(cache.get(4L, val));
    ASSERT_EQ(val, 15UL);
    ASSERT_EQ(cache.get_total_query_times(), ++total_get);
    ASSERT_EQ(cache.get_hit_query_times(), ++success_get);

    ASSERT_TRUE(cache.get(5L, val));
    ASSERT_EQ(val, 16UL);
    ASSERT_EQ(cache.get_total_query_times(), ++total_get);
    ASSERT_EQ(cache.get_hit_query_times(), ++success_get);

    ASSERT_TRUE(cache.get(6L, val));
    ASSERT_EQ(val, 17UL);
    ASSERT_EQ(cache.get_total_query_times(), ++total_get);
    ASSERT_EQ(cache.get_hit_query_times(), ++success_get);

    cache.erase(5UL);
    ASSERT_EQ(cache.get_mem_size_used(), 16UL);
    ASSERT_EQ(cache.size(), --element);

    ASSERT_FALSE(cache.get(5L, val));
    ASSERT_EQ(cache.get_total_query_times(), ++total_get);
    ASSERT_EQ(cache.get_hit_query_times(), success_get);

    ASSERT_FALSE(cache.put(8L, 36L, 15L));
    ASSERT_EQ(cache.get_mem_size_used(), 16UL);
    ASSERT_EQ(cache.size(), element);
}

TEST(LRUCacheTest, multiThread) {
    size_t max_mem_size = 24;
    antflash::LRUCache<std::string, std::string> cache(s_bucket_size, max_mem_size);

    ASSERT_TRUE(cache.put("1", 8L, "A"));
    ASSERT_TRUE(cache.put("2", 8L, "B"));
    ASSERT_TRUE(cache.put("3", 8L, "C"));

    constexpr size_t data_size = 1000000L;

    auto dest_time = std::chrono::system_clock::now()
                     + std::chrono::milliseconds(100);

    std::thread t1([&cache, &dest_time]() {
        std::this_thread::sleep_until(dest_time);

        for (size_t i = 0; i < data_size; ++i) {
            std::string val;
            if (cache.get("1", val)) {
                ASSERT_TRUE(val == "A" || val == "D");
            }
        }
    });

    std::thread t2([&cache, &dest_time]() {
        std::this_thread::sleep_until(dest_time);

        std::random_device rd;
        for (size_t i = 0; i < data_size; ++i) {
            std::string key("1");
            std::string val;
            size_t tmp = rd() % 3;
            if (tmp == 1) {
                key = "2";
            } else if (tmp == 2) {
                key = "3";
            }
            if (cache.get(key, val)) {
                if (key == "1") {
                    ASSERT_TRUE(val == "A" || val == "D");
                } else if (key == "2") {
                    ASSERT_TRUE(val == "B" || val == "E");
                } else {
                    ASSERT_TRUE(val == "C" || val == "F");
                }
            }
        }
    });

    std::thread t3([&cache, &dest_time]() {
        std::this_thread::sleep_until(dest_time);

        std::random_device rd;
        for (size_t i = 0; i < data_size; ++i) {
            size_t tmp = rd();
            if (tmp % 100 == 0) {
                if (tmp % 2 == 0) {
                    cache.exchange("1", 8L, tmp % 5 == 0 ? "D" : "A");
                } else {
                    cache.put("2", 8L, tmp % 7 == 0 ? "B" : "E");
                }
            }
        }
    });

    std::thread t4([&cache, &dest_time]() {
        std::this_thread::sleep_until(dest_time);

        std::random_device rd;
        char key[64];
        for (size_t i = 0; i < data_size; ++i) {
            size_t tmp = rd();
            if (tmp % 1000 == 0) {
                if (tmp % 2 == 0) {
                    cache.exchange("3", 8L, tmp % 5 == 0 ? "F" : "C");
                } else if (tmp > 3) {
                    sprintf(key, "%lu", tmp);
                    ASSERT_TRUE(cache.put(key, 8L, key));
                }
            }
        }
    });

    std::thread t5([&cache, &dest_time]() {
        std::this_thread::sleep_until(dest_time);

        std::random_device rd;
        for (size_t i = 0; i < data_size; ++i) {
            size_t tmp = rd();
            if (tmp % 1024 == 0) {
                if (tmp % 2 == 0) {
                    cache.erase("3");
                } else {
                    cache.erase("1");
                }
            }
        }
    });

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();

    std::cout << "hit cache rate:" << cache.get_hit_query_times() * 1.0 /
            cache.get_total_query_times() << std::endl;
}
