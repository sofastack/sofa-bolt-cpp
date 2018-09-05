// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/16.
//

#ifndef RPC_COMMON_LOCKFREE_QUEUE_H
#define RPC_COMMON_LOCKFREE_QUEUE_H

#include <atomic>
#include <cstdlib>
#include <cassert>
#include <thread>
#include "common_defines.h"

namespace antflash {

/**
 * Multiple producer and single consumer concurrent queue with fixed size,
 * only worked properly in X86 architecture currently.The fixed capacity
 * must be power of 2.
 * @tparam T : queue data type
 */
template <typename T>
class MPSCQueue {

static_assert(std::is_pointer<T>::value, "T must be a pointer");

public:
    MPSCQueue(size_t capacity) :
            _capacity(capacity),
            _mask(capacity - 1),
            _buffer(static_cast<T*>(std::malloc(sizeof(T) * _capacity))),
            _read_idx(0), _write_prepare_idx(0), _write_idx(0) {
        assert((_capacity & _mask) == 0);//power of 2
    }

    ~MPSCQueue() {
        std::free(_buffer);
    }

    //multiple producer
    bool push(T& val) {
        auto write = _write_prepare_idx.load(std::memory_order_acquire);

        do {
            auto read = _read_idx.load(std::memory_order_acquire);
            if (write >= read + _capacity) {
                //queue is full
                return false;
            }
        } while (!_write_prepare_idx.compare_exchange_strong(
                write, write + 1, std::memory_order_acq_rel, std::memory_order_relaxed));

        _buffer[write & _mask] = val;

        //this will block write thread for a while waiting for index going forward
        auto write_commit = write;
        size_t count = 0;
        while(!_write_idx.compare_exchange_strong(
                write_commit, write_commit + 1, 
                std::memory_order_acq_rel, std::memory_order_relaxed)) {
            if (++count > 100) {
                count = 0;
                std::this_thread::yield();
            }
        }

        return true;
    }

    //single consumer
    bool pop(T& val) {
        auto read = _read_idx.load(std::memory_order_acquire);
        if (read == _write_idx.load(std::memory_order_acquire)) {
            //queue is empty
            return false;
        }
        
        val = _buffer[read & _mask];
        _read_idx.store(read + 1, std::memory_order_release);
        return true;
    }

    size_t size() const {
        const size_t w = _write_idx.load(std::memory_order_relaxed);
        const size_t r = _read_idx.load(std::memory_order_relaxed);
        return w <= r ? 0 : (w - r);
    }

    size_t capacity() const {
        return _capacity;
    }

private:
    size_t _capacity;
    size_t _mask;
    T* _buffer;
    alignas(SIZE_OF_AVOID_FALSE_SHARING) std::atomic<size_t> _read_idx;
    alignas(SIZE_OF_AVOID_FALSE_SHARING) std::atomic<size_t> _write_prepare_idx;
    alignas(SIZE_OF_AVOID_FALSE_SHARING)std::atomic<size_t> _write_idx;
    char _padding[SIZE_OF_AVOID_FALSE_SHARING - sizeof(std::atomic<size_t>)];
};

/**
 * Single producer and multiple consumer lock free queue with fixed size,
 * only worked properly in X86 architecture currently. The fixed capacity
 * must be power of 2.
 * @tparam T : queue data type
 */
template <typename T>
class SPMCQueue {

static_assert(std::is_pointer<T>::value, "T must be a pointer");

public:
    SPMCQueue(size_t capacity) :
            _capacity(capacity),
            _mask(capacity- 1),
            _buffer(static_cast<T*>(std::malloc(sizeof(T) * _capacity))),
            _read_idx(0), _write_idx(0) {
        assert((_capacity & _mask) == 0);//power of 2
    }

    ~SPMCQueue() {
        std::free(_buffer);
    }

    //single producer
    template<typename ...Args>
    bool push(Args&& ...args) {
        auto write = _write_idx.load(std::memory_order_acquire);
        auto read = _read_idx.load(std::memory_order_acquire);
        if (write >= read + _capacity) {
            //queue is full
            return false;
        }

        new (&_buffer[write & _mask]) T (std::forward<Args>(args)...);

        _write_idx.store(write + 1, std::memory_order_release);
        return true;
    }

    //multiple consumer
    bool pop(T& val) {
        auto read = _read_idx.load(std::memory_order_acquire);

        do {
            if (read == _write_idx.load(std::memory_order_acquire)) {
                //queue is empty
                return false;
            }
        
            //copy here, not move
            val = _buffer[read & _mask];
        } while (!_read_idx.compare_exchange_strong(
                    read, read + 1, std::memory_order_acq_rel, std::memory_order_relaxed));

        return true;
    }

    size_t size() const {
        const size_t w = _write_idx.load(std::memory_order_relaxed);
        const size_t r = _read_idx.load(std::memory_order_relaxed);
        return w <= r ? 0 : (w - r);
    }

    size_t capacity() const {
        return _capacity;
    }

private:
    size_t _capacity;
    size_t _mask;
    T* _buffer;
    alignas(SIZE_OF_AVOID_FALSE_SHARING) std::atomic<size_t> _read_idx;
    alignas(SIZE_OF_AVOID_FALSE_SHARING)std::atomic<size_t> _write_idx;
    char _padding[SIZE_OF_AVOID_FALSE_SHARING - sizeof(std::atomic<size_t>)];
};

/**
 * Single producer and single consumer wait free queue with fixed size,
 * The fixed capacity must be power of 2, same with boost::waitfree_queue
 *
 * @tparam T : queue data type
 */
template <typename T>
class SPSCQueue {
public:
    SPSCQueue(size_t capacity) :
            _capacity(capacity),
            _mask(capacity- 1),
            _buffer(static_cast<T*>(std::malloc(sizeof(T) * _capacity))),
            _read_idx(0), _write_idx(0) {
        assert((_capacity & _mask) == 0);//power of 2
    }

    ~SPSCQueue() {
        if (!std::is_trivially_destructible<T>::value) {
            auto read = _read_idx.load(std::memory_order_acquire);
            auto write = _write_idx.load(std::memory_order_acquire);
            while (read != write) {
                _buffer[read & _mask].~T();
                ++read;
            }
        }

        std::free(_buffer);
    }

    //single producer
    template<typename ...Args>
    bool push(Args&& ...args) {
        auto write = _write_idx.load(std::memory_order_acquire);
        if (write >= _read_idx.load(std::memory_order_acquire) + _capacity) {
            //full queue
            return false;
        }

        new (&_buffer[write & _mask]) T (std::forward<Args>(args)...);
        _write_idx.store(write + 1, std::memory_order_release);
        return true;
    }

    //single consumer
    bool pop(T& val) {
        auto read = _read_idx.load(std::memory_order_acquire);
        if (read == _write_idx.load(std::memory_order_acquire)) {
            //queue is empty
            return false;
        }

        val = std::move(_buffer[read & _mask]);
        _buffer[read & _mask].~T();
        _read_idx.store(read + 1, std::memory_order_release);
        return true;
    }

    //only for single comsumer, should check empty first
    const T& front() const {
        auto read = _read_idx.load(std::memory_order_acquire);
        return _buffer[read & _mask];
    }

    size_t size() const {
        const size_t w = _write_idx.load(std::memory_order_relaxed);
        const size_t r = _read_idx.load(std::memory_order_relaxed);
        return w <= r ? 0 : (w - r);
    }

    size_t capacity() const {
        return _capacity;
    }

private:
    size_t _capacity;
    size_t _mask;
    T* _buffer;
    alignas(SIZE_OF_AVOID_FALSE_SHARING) std::atomic<size_t> _read_idx;
    alignas(SIZE_OF_AVOID_FALSE_SHARING)std::atomic<size_t> _write_idx;
    char _padding[SIZE_OF_AVOID_FALSE_SHARING - sizeof(std::atomic<size_t>)];
};
}

#endif //RPC_COMMON_LOCKFREE_QUEUE_H
