//
// Created by zhenggu.xwt on 18/4/16.
//

#include <common/lockfree_queue.h>
#include <gtest/gtest.h>
#include <thread>
#include <mutex>
#include <map>

TEST(MPSCQueueTest, singleThread) {
    antflash::MPSCQueue<size_t*> queue(2);
    ASSERT_EQ(queue.size(), 0UL);
    size_t a = 1234;
    size_t* ap = &a;
    GTEST_ASSERT_EQ(queue.push(ap), true);
    ASSERT_EQ(queue.size(), 1UL);
    size_t* bp = nullptr;
    GTEST_ASSERT_EQ(queue.pop(bp), true);
    GTEST_ASSERT_EQ(bp, ap);
    GTEST_ASSERT_EQ(*bp, a);
    size_t c = 4321;
    size_t* cp = &c;
    GTEST_ASSERT_EQ(queue.push(ap), true);
    GTEST_ASSERT_EQ(queue.push(cp), true);
    GTEST_ASSERT_EQ(queue.push(ap), false);
    GTEST_ASSERT_EQ(queue.push(cp), false);
    GTEST_ASSERT_EQ(queue.pop(bp), true);
    GTEST_ASSERT_EQ(bp, ap);
    GTEST_ASSERT_EQ(*bp, a);
    GTEST_ASSERT_EQ(queue.pop(bp), true);
    GTEST_ASSERT_EQ(bp, cp);
    GTEST_ASSERT_EQ(*bp, c);
}

TEST(MPSCQueueTest, singleProducer) {
    antflash::MPSCQueue<size_t*> queue(4);
    ASSERT_EQ(queue.capacity(), 4UL);
    ASSERT_EQ(queue.size(), 0UL);

    constexpr size_t DATA_SIZE = 10000;
    auto dest_time = std::chrono::system_clock::now()
                    + std::chrono::milliseconds(100);
    size_t data[DATA_SIZE];

    std::thread producer([&queue, &dest_time, &data]() {
        std::this_thread::sleep_until(dest_time);
        size_t* p = nullptr;
        for (size_t i = 0; i < DATA_SIZE;) {
            data[i] = i;
            p = &data[i];
            auto ret = queue.push(p);
            if (ret) {
                ++i;
            }
        }
    });

    std::thread consumer([&queue, &dest_time, &data]() {
        std::this_thread::sleep_until(dest_time);
        size_t* p = nullptr;
        for (size_t i = 0; i < DATA_SIZE;) {
            auto ret = queue.pop(p);
            if (ret) {
                GTEST_ASSERT_EQ(p, &data[i]);
                GTEST_ASSERT_EQ(*p, i);
                ++i;
            }
        }
    });

    producer.join();
    consumer.join();
}

TEST(SPSCQueueTest, singleProducer) {
    antflash::SPSCQueue<size_t*> queue(4);
    ASSERT_EQ(queue.capacity(), 4UL);
    ASSERT_EQ(queue.size(), 0UL);

    size_t a = 1234;
    size_t* ap = &a;
    GTEST_ASSERT_EQ(queue.push(ap), true);
    ASSERT_EQ(queue.size(), 1UL);
    size_t* bp = nullptr;
    GTEST_ASSERT_EQ(queue.pop(bp), true);
    GTEST_ASSERT_EQ(bp, ap);
    GTEST_ASSERT_EQ(*bp, a);

    constexpr size_t DATA_SIZE = 10000;
    auto dest_time = std::chrono::system_clock::now()
                     + std::chrono::milliseconds(100);
    size_t data[DATA_SIZE];

    std::thread producer([&queue, &dest_time, &data]() {
        std::this_thread::sleep_until(dest_time);
        size_t* p = nullptr;
        for (size_t i = 0; i < DATA_SIZE;) {
            data[i] = i;
            p = &data[i];
            auto ret = queue.push(p);
            if (ret) {
                ++i;
            }
        }
    });

    std::thread consumer([&queue, &dest_time, &data]() {
        std::this_thread::sleep_until(dest_time);
        size_t* p = nullptr;
        for (size_t i = 0; i < DATA_SIZE;) {
            auto ret = queue.pop(p);
            if (ret) {
                GTEST_ASSERT_EQ(p, &data[i]);
                GTEST_ASSERT_EQ(*p, i);
                ++i;
            }
        }
    });

    producer.join();
    consumer.join();
}

TEST(SPMCQueueTest, singleProducer) {
    antflash::SPMCQueue<size_t*> queue(4);
    ASSERT_EQ(queue.capacity(), 4UL);
    ASSERT_EQ(queue.size(), 0UL);

    size_t a = 1234;
    size_t* ap = &a;
    GTEST_ASSERT_EQ(queue.push(ap), true);
    ASSERT_EQ(queue.size(), 1UL);
    size_t* bp = nullptr;
    GTEST_ASSERT_EQ(queue.pop(bp), true);
    GTEST_ASSERT_EQ(bp, ap);
    GTEST_ASSERT_EQ(*bp, a);

    constexpr size_t DATA_SIZE = 10000;
    auto dest_time = std::chrono::system_clock::now()
                     + std::chrono::milliseconds(100);
    size_t data[DATA_SIZE];

    std::thread producer([&queue, &dest_time, &data]() {
        std::this_thread::sleep_until(dest_time);
        size_t* p = nullptr;
        for (size_t i = 0; i < DATA_SIZE;) {
            data[i] = i;
            p = &data[i];
            auto ret = queue.push(p);
            if (ret) {
                ++i;
            }
        }
    });

    std::thread consumer([&queue, &dest_time, &data]() {
        std::this_thread::sleep_until(dest_time);
        size_t* p = nullptr;
        for (size_t i = 0; i < DATA_SIZE;) {
            auto ret = queue.pop(p);
            if (ret) {
                GTEST_ASSERT_EQ(p, &data[i]);
                GTEST_ASSERT_EQ(*p, i);
                ++i;
            }
        }
    });

    producer.join();
    consumer.join();
}


TEST(MPSCQueueTest, multiProducer) {
    antflash::MPSCQueue<size_t*> queue(128);

    constexpr size_t PRODUCERS = 3;
    constexpr size_t DATA_SIZE = 10000;
    auto dest_time = std::chrono::system_clock::now()
                     + std::chrono::milliseconds(100);
    size_t* data = new size_t[DATA_SIZE * PRODUCERS];

    std::thread* producers[PRODUCERS];
    for (size_t idx = 0; idx < PRODUCERS; ++idx) {
        producers[idx] = new std::thread([PRODUCERS, idx, &queue, &dest_time, &data]() {
            std::this_thread::sleep_until(dest_time);
            size_t* p = nullptr;
            for (size_t i = 0; i < DATA_SIZE;) {
                size_t offset = i * PRODUCERS + idx;
                data[offset] = offset;
                p = &data[offset];
                auto ret = queue.push(p);
                if (ret) {
                    ++i;
                }
            }
        });
    }

    std::thread consumer([&queue, &dest_time, &data]() {
        std::this_thread::sleep_until(dest_time);
        std::map<size_t, size_t*> results;
        size_t* p = nullptr;
        for (size_t i = 0; i < DATA_SIZE * PRODUCERS;) {
            auto ret = queue.pop(p);
            if (ret) {
                results.emplace(*p, p);
                ++i;
            }
        }
        for (auto& pair : results) {
            GTEST_ASSERT_EQ(*pair.second, pair.first);
            GTEST_ASSERT_EQ(pair.second, &data[pair.first]);
        }
    });

    for (auto& producer : producers) {
        producer->join();
        delete producer;
    }
    consumer.join();

    delete [] data;
}

template <typename T>
class MutexQueue {
    static_assert(std::is_pointer<T>::value, "T must be a pointer");

public:
    MutexQueue(size_t capacity) :
    _capacity(capacity),
    _mask(capacity- 1),
    _buffer(static_cast<T*>(std::malloc(sizeof(T) * _capacity))),
    _read_idx(0), _write_idx(0) {
        assert((_capacity & _mask) == 0);//power of 2
    }

    ~MutexQueue() {
        std::free(_buffer);
    }
    //multiple producer
    bool push(T& val) {
        std::lock_guard<std::mutex> lock(_mtx);

        if (_write_idx >= _read_idx + _capacity) {
                //queue is full
                return false;
        }

        _buffer[_write_idx & _mask] = val;
        ++_write_idx;

        return true;
    }

    //single consumer
    bool pop(T& val) {
        std::lock_guard<std::mutex> lock(_mtx);
        if (_read_idx == _write_idx) {
            //queue is empty
            return false;
        }

        val = _buffer[_read_idx & _mask];
        ++_read_idx;

        return true;
    }

    size_t capacity() const {
        return _capacity;
    }

private:
    size_t _capacity;
    size_t _mask;
    T* _buffer;
    size_t _read_idx;
    size_t _write_idx;
    std::mutex _mtx;
};

TEST(MPSCQueueTest, multiProducerMutex) {
    MutexQueue<size_t*> queue(128);

    constexpr size_t PRODUCERS = 3;
    constexpr size_t DATA_SIZE = 10000;
    auto dest_time = std::chrono::system_clock::now()
                     + std::chrono::milliseconds(100);
    size_t* data = new size_t[DATA_SIZE * PRODUCERS];

    std::thread* producers[PRODUCERS];
    for (size_t idx = 0; idx < PRODUCERS; ++idx) {
        producers[idx] = new std::thread([PRODUCERS, idx, &queue, &dest_time, &data]() {
            std::this_thread::sleep_until(dest_time);
            size_t* p = nullptr;
            for (size_t i = 0; i < DATA_SIZE;) {
                size_t offset = i * PRODUCERS + idx;
                data[offset] = offset;
                p = &data[offset];
                auto ret = queue.push(p);
                if (ret) {
                    ++i;
                }
            }
        });
    }

    std::thread consumer([&queue, &dest_time, &data]() {
        std::this_thread::sleep_until(dest_time);
        std::map<size_t, size_t*> results;
        size_t* p = nullptr;
        for (size_t i = 0; i < DATA_SIZE * PRODUCERS;) {
            auto ret = queue.pop(p);
            if (ret) {
                results.emplace(*p, p);
                ++i;
            }
        }
        for (auto& pair : results) {
            GTEST_ASSERT_EQ(*pair.second, pair.first);
            GTEST_ASSERT_EQ(pair.second, &data[pair.first]);
        }
    });

    for (auto& producer : producers) {
        producer->join();
        delete producer;
    }
    consumer.join();

    delete [] data;
}
