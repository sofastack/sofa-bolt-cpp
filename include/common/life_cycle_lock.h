// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/16.
//

#ifndef RPC_COMMON_LIFE_CYCLE_LOCK_H
#define RPC_COMMON_LIFE_CYCLE_LOCK_H

#include <atomic>
#include <thread>

namespace antflash {

class LifeCycleLock {
public:
    enum : int32_t {
        SHARED = 4,
        UPGRADED = 2,
        EXCLUSIVE = 1
    };

    LifeCycleLock() : _bits(0) {}
    ~LifeCycleLock() {}

    LifeCycleLock(LifeCycleLock&) = delete;
    LifeCycleLock& operator=(LifeCycleLock&) = delete;

    void share() {
        int64_t count = 0;
        while (!tryShared()) {
            if (++count > 1000) {
                count = 0;
                std::this_thread::yield();
            }
        }
    }

    //Try to share this lock, this can be fail if the lock is in exclusive mode
    //or someone is trying to upgrade lock to exclusive mode.
    bool tryShared() {
        int32_t value = _bits.fetch_add(SHARED, std::memory_order_acq_rel);
        if (value & (UPGRADED | EXCLUSIVE)) {
            _bits.fetch_add(-SHARED, std::memory_order_release);
            return false;
        }

        return true;
    }

    void upgrade() {
        int64_t count = 0;
        while (!tryUpgrade()) {
            if (++count > 1000) {
                count = 0;
                std::this_thread::yield();
            }
        }
    }

    //Try to make this lock exclusive, before that we need to upgrade lock
    //first to acquire it first, when failed, we cannot flip the UPGRADED
    //bit back, as in this case there is either another upgrade lock or a
    // exclusive lock, if it's a exclusive lock, the bit will get cleared
    // up when that lock's done.
    bool tryUpgrade() {
        int32_t value = _bits.fetch_or(UPGRADED, std::memory_order_acq_rel);
        return ((value & EXCLUSIVE) == 0);
    }

    bool tryUpgradeNonReEntrant() {
        int32_t value = _bits.fetch_or(UPGRADED, std::memory_order_acq_rel);
        return ((value & (UPGRADED | EXCLUSIVE)) == 0);
    }

    void exclusive() {
        int64_t count = 0;
        while (!tryExclusive()) {
            if (++count > 1000) {
                count = 0;
                std::this_thread::yield();
            }
        }
    }

    //Try to unlock upgrade and make lock exclusive atomically
    bool tryExclusive()
    {
        int32_t expect = UPGRADED;
        return _bits.compare_exchange_strong(
                expect, EXCLUSIVE, std::memory_order_acq_rel);
    }

    void releaseShared() {
        _bits.fetch_add(-SHARED, std::memory_order_release);
    }

    void releaseExclusive()
    {
        _bits.fetch_and(~(EXCLUSIVE | UPGRADED), std::memory_order_release);
    }

    int32_t record() {
        return _bits.load();
    }

private:
    std::atomic<int32_t> _bits;
};


class LifeCycleShareGuard {
public:
    LifeCycleShareGuard(LifeCycleLock& lock) : _lock(&lock) {
        _shared = _lock->tryShared();
    }

    ~LifeCycleShareGuard() {
        if (_shared) {
            _lock->releaseShared();
        }
    }

    inline bool shared() const {
        return _shared;
    }

private:
    LifeCycleLock* _lock;
    bool _shared;
};

}

#endif //RPC_COMMON_LIFE_CYCLE_LOCK_H
