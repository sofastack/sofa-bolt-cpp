// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/5/21.
//

#ifndef RPC_COMMON_CONCURRENT_HASH_MAP_H
#define RPC_COMMON_CONCURRENT_HASH_MAP_H

#include <utility>
#include <iterator>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include "common/life_cycle_lock.h"

namespace antflash {

template <typename K, typename V>
struct HashNode
{
    K key;
    V value;
    HashNode* hash_next;

    bool is_intrusive() const {
        return false;
    }

    template <typename ...Args>
    HashNode(const K& key, Args&& ...args) :
            key(key), value(std::forward<Args>(args)...), hash_next(nullptr) {}
};

inline size_t nextBucketNum(size_t n) {
    //Copy from std::hash_table
    static const size_t primes[] = {
            2ul, 3ul, 5ul, 7ul, 11ul, 13ul, 17ul, 19ul, 23ul, 29ul, 31ul,
            37ul, 41ul, 43ul, 47ul, 53ul, 59ul, 61ul, 67ul, 71ul, 73ul, 79ul,
            83ul, 89ul, 97ul, 103ul, 109ul, 113ul, 127ul, 137ul, 139ul, 149ul,
            157ul, 167ul, 179ul, 193ul, 199ul, 211ul, 227ul, 241ul, 257ul,
            277ul, 293ul, 313ul, 337ul, 359ul, 383ul, 409ul, 439ul, 467ul,
            503ul, 541ul, 577ul, 619ul, 661ul, 709ul, 761ul, 823ul, 887ul,
            953ul, 1031ul, 1109ul, 1193ul, 1289ul, 1381ul, 1493ul, 1613ul,
            1741ul, 1879ul, 2029ul, 2179ul, 2357ul, 2549ul, 2753ul, 2971ul,
            3209ul, 3469ul, 3739ul, 4027ul, 4349ul, 4703ul, 5087ul, 5503ul,
            5953ul, 6427ul, 6949ul, 7517ul, 8123ul, 8783ul, 9497ul, 10273ul,
            11113ul, 12011ul, 12983ul, 14033ul, 15173ul, 16411ul, 17749ul,
            19183ul, 20753ul, 22447ul, 24281ul, 26267ul, 28411ul, 30727ul,
            33223ul, 35933ul, 38873ul, 42043ul, 45481ul, 49201ul, 53201ul,
            57557ul, 62233ul, 67307ul, 72817ul, 78779ul, 85229ul, 92203ul,
            99733ul, 107897ul, 116731ul, 126271ul, 136607ul, 147793ul,
            159871ul, 172933ul, 187091ul, 202409ul, 218971ul, 236897ul,
            256279ul, 277261ul, 299951ul, 324503ul, 351061ul, 379787ul,
            410857ul, 444487ul, 480881ul, 520241ul, 562841ul, 608903ul,
            658753ul, 712697ul, 771049ul, 834181ul, 902483ul, 976369ul,
            1056323ul, 1142821ul, 1236397ul, 1337629ul, 1447153ul, 1565659ul,
            1693859ul, 1832561ul, 1982627ul, 2144977ul, 2320627ul, 2510653ul,
            2716249ul, 2938679ul, 3179303ul, 3439651ul, 3721303ul, 4026031ul,
            4355707ul, 4712381ul, 5098259ul, 5515729ul, 5967347ul, 6456007ul,
            6984629ul, 7556579ul, 8175383ul, 8844859ul, 9569143ul, 10352717ul,
            11200489ul, 12117689ul, 13109983ul, 14183539ul, 15345007ul,
            16601593ul, 17961079ul, 19431899ul, 21023161ul, 22744717ul,
            24607243ul, 26622317ul, 28802401ul, 31160981ul, 33712729ul,
            36473443ul, 39460231ul, 42691603ul, 46187573ul, 49969847ul,
            54061849ul, 58488943ul, 63278561ul, 68460391ul, 74066549ul,
            80131819ul, 86693767ul, 93793069ul, 101473717ul, 109783337ul,
            118773397ul, 128499677ul, 139022417ul, 150406843ul, 162723577ul,
            176048909ul, 190465427ul, 206062531ul, 222936881ul, 241193053ul,
            260944219ul, 282312799ul, 305431229ul, 330442829ul, 357502601ul,
            386778277ul, 418451333ul, 452718089ul, 489790921ul, 529899637ul,
            573292817ul, 620239453ul, 671030513ul, 725980837ul, 785430967ul,
            849749479ul, 919334987ul, 994618837ul, 1076067617ul, 1164186217ul,
            1259520799ul, 1362662261ul, 1474249943ul, 1594975441ul,
            1725587117ul, 1866894511ul, 2019773507ul, 2185171673ul,
            2364114217ul, 2557710269ul, 2767159799ul, 2993761039ul,
            3238918481ul, 3504151727ul, 3791104843ul, 4101556399ul,
            4294967291ul
    };

    static constexpr size_t length =
            sizeof(primes) / sizeof(size_t);

    const size_t* bound =
            std::lower_bound(primes, primes + length - 1, n);

    return *bound;
}

template<typename K, typename V,
        typename NodeType = HashNode<K, V>,
        typename HashFunc = std::hash<K>>
class ConcurrentHashMap {
public:
    typedef NodeType* NodePtr;
    typedef V value_type;
    typedef K key_type;

    ConcurrentHashMap(size_t bucket_hint) : _element_size(0) {
        _bucket_size = nextBucketNum(bucket_hint);
        _buckets = allocate_buckets(_bucket_size);

        _locks = new LifeCycleLock[_bucket_size];
        _lock_size = _bucket_size;
    }

    ConcurrentHashMap(size_t bucket_hint, size_t lock_num) : _element_size(0) {
        _bucket_size = nextBucketNum(bucket_hint);
        _buckets = allocate_buckets(_bucket_size);

        _locks = new LifeCycleLock[lock_num];
        _lock_size = lock_num;
    }

    virtual ~ConcurrentHashMap() {
        destroy();
    }

    template<typename ...Args>
    bool put(const K& key, Args&& ...args) {
        LifeCycleLock* lock = nullptr;
        NodePtr& n = get_entry(key, lock);
        while (true) {
            if (lock->tryUpgrade() && lock->tryExclusive()) {
                break;
            }
        }

        auto ret = put_nolock(n, key, std::forward<Args>(args)...);

        lock->releaseExclusive();

        return ret;
    }

    bool exchange(const K& key, V& val) {
        LifeCycleLock* lock = nullptr;
        NodePtr& n = get_entry(key, lock);
        while (true) {
            if (lock->tryUpgrade() && lock->tryExclusive()) {
                break;
            }
        }

        auto ret = exchange_nolock(n, key, val);

        lock->releaseExclusive();

        return ret;
    }

    bool get(const K& key, V& val) {
        LifeCycleLock* lock = nullptr;
        NodePtr& n = get_entry(key, lock);
        lock->share();

        auto ret = get_nolock(n, key, val);

        lock->releaseShared();

        return ret;
    }

    bool erase(const K& key) {
        LifeCycleLock* lock = nullptr;
        NodePtr& n = get_entry(key, lock);
        while (true) {
            if (lock->tryUpgrade() && lock->tryExclusive()) {
                break;
            }
        }

        auto ret = erase_nolock(n, key);

        lock->releaseExclusive();

        return ret;
    }

    //For unit test
    size_t getHash(const K& key) const {
        return _hasher(key);
    }

    inline const size_t size() const {
        return _element_size;
    }

    inline const size_t bucketSize() const {
        return _bucket_size;
    }

private:
    NodePtr& get_entry(const K &key, LifeCycleLock *&lock) const {
        size_t k = _hasher(key);
        size_t offset = k % _bucket_size;
        uint64_t lock_offset = offset % _lock_size;
        lock = _locks + lock_offset;
        return _buckets[offset];
    }

    template<typename ...Args>
    bool put_nolock(NodePtr& n, const K& key, Args&& ...args) {
        NodePtr* p = &n;
        while (*p) {
            if ((*p)->key == key) {
                return false;
            }
            p = &((*p)->hash_next);
        }
        *p = new HashNode<K, V>(key, std::forward<Args>(args)...);
        ++_element_size;
        return true;
    }

    bool intrusive_put_nolock(NodePtr& n, const K& key, NodePtr val) {
        NodePtr* p = &n;
        while (*p) {
            if ((*p)->key == key) {
                return false;
            }
            p = &((*p)->hash_next);
        }
        *p = val;
        ++_element_size;
        return true;
    }

    bool exchange_nolock(NodePtr& n, const K& key, V& val) {
        NodePtr* p = &n;
        while (*p) {
            if ((*p)->key == key) {
                std::swap((*p)->value, val);
                return true;
            }
            p = &((*p)->hash_next);
        }
        *p = new HashNode<K, V>(key, val);
        ++_element_size;
        return false;
    }

    bool intrusive_exchange_nolock(NodePtr& n, const K& key, NodePtr& val) {
        NodePtr* p = &n;
        while (*p) {
            if ((*p)->key == key) {
                NodePtr tmp = *p;
                *p = val;
                val = tmp;
                return true;
            }
            p = &((*p)->hash_next);
        }
        *p = val;
        ++_element_size;
        return false;
    }

    bool get_nolock(NodePtr& n, const K& key, V& val) {
        NodePtr* p = &n;
        while (*p) {
            if ((*p)->key == key) {
                val = (*p)->value;
                return true;
            }
            p = &((*p)->hash_next);
        }

        return false;
    }

    bool intrusive_get_nolock(NodePtr& n, const K& key, NodePtr& val) {
        NodePtr* p = &n;
        while (*p) {
            if ((*p)->key == key) {
                val = *p;
                return true;
            }
            p = &((*p)->hash_next);
        }

        return false;
    }

    bool erase_nolock(NodePtr& n, const K& key) {
        NodePtr* p = &n;
        while (*p) {
            if ((*p)->key == key) {
                NodePtr q = *p;
                *p = (*p)->hash_next;
                if (!q->is_intrusive()) {
                    delete q;
                }
                --_element_size;
                return true;
            }
            p = &((*p)->hash_next);
        }

        return false;
    }

    bool intrusive_erase_nolock(NodePtr& n, const K& key, NodePtr& val) {
        NodePtr* p = &n;
        while (*p) {
            if ((*p)->key == key) {
                NodePtr q = *p;
                *p = (*p)->hash_next;
                val = q;
                --_element_size;
                return true;
            }
            p = &((*p)->hash_next);
        }

        return false;
    }

    NodePtr* allocate_buckets(size_t n) {
        return static_cast<NodePtr*>(std::calloc(n, sizeof(NodePtr)));
    }

    void deallocate_buckets(NodePtr* n, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            NodePtr p = n[i];
            while (p) {
                NodePtr q = p;
                p = p->hash_next;

                if (!q->is_intrusive()) {
                    delete q;
                }
            }
        }
        std::free(n);
    }

    NodePtr* _buckets;
    LifeCycleLock* _locks;
    size_t _lock_size;
    size_t _bucket_size;
    size_t _element_size;
    HashFunc _hasher;

/**
 * Following for LRU cache
 */
protected:
    template <typename F1, typename F2, typename F3, typename F4, typename F5>
    ConcurrentHashMap(size_t bucket_hint, F1&& f1, F2&& f2, F3&& f3, F4&& f4, F5&& f5) :
            _element_size(0), _intrusive_put_preprocess(std::forward<F1>(f1)),
            _intrusive_get_postprocess(std::forward<F2>(f2)),
            _intrusive_put_postprocess(std::forward<F3>(f3)),
            _intrusive_exchange_postprocess(std::forward<F4>(f4)),
            _intrusive_erase_postprocess(std::forward<F5>(f5))
    {
        _bucket_size = nextBucketNum(bucket_hint);
        _buckets = allocate_buckets(_bucket_size);

        _locks = new LifeCycleLock[_bucket_size];
        _lock_size = _bucket_size;
    }

    bool intrusive_get(const K& key, V& val) {
        LifeCycleLock* lock = nullptr;
        NodePtr& n = get_entry(key, lock);
        lock->share();

        NodePtr p = nullptr;
        auto ret = intrusive_get_nolock(n, key, p);
        if (_intrusive_get_postprocess) {
            _intrusive_get_postprocess(p, ret);
        }
        if (ret) {
            val = p->value;
        }

        lock->releaseShared();

        return ret;
    }

    bool intrusive_put(const K& key, NodePtr val) {
        LifeCycleLock* lock = nullptr;
        NodePtr& n = get_entry(key, lock);
        while (true) {
            if (lock->tryUpgrade() && lock->tryExclusive()) {
                break;
            }
        }

        if (_intrusive_put_preprocess) {
            if (!_intrusive_put_preprocess(val)) {
                lock->releaseExclusive();
                return false;
            }
        }

        auto ret = intrusive_put_nolock(n, key, val);

        if (_intrusive_put_postprocess) {
            _intrusive_put_postprocess(val, ret);
        }

        lock->releaseExclusive();

        return ret;
    }

    bool intrusive_exchange(const K& key, NodePtr& val) {
        LifeCycleLock* lock = nullptr;
        NodePtr& n = get_entry(key, lock);
        while (true) {
            if (lock->tryUpgrade() && lock->tryExclusive()) {
                break;
            }
        }

        NodePtr to_exchange = val;
        auto ret = intrusive_exchange_nolock(n, key, val);

        if (_intrusive_exchange_postprocess) {
            _intrusive_exchange_postprocess(to_exchange, val, ret);
        }

        lock->releaseExclusive();

        return ret;
    }

    bool intrusive_erase(const K& key) {
        LifeCycleLock* lock = nullptr;
        NodePtr& n = get_entry(key, lock);
        while (true) {
            if (lock->tryUpgrade() && lock->tryExclusive()) {
                break;
            }
        }

        NodePtr val = nullptr;
        auto ret = intrusive_erase_nolock(n, key, val);
        if (_intrusive_erase_postprocess) {
            _intrusive_erase_postprocess(val, ret);
        }

        lock->releaseExclusive();

        return ret;
    }

    bool erase_nolock(const K& key) {
        LifeCycleLock* lock = nullptr;
        NodePtr& n = get_entry(key, lock);
        auto ret = erase_nolock(n, key);
        return ret;
    }

    void destroy() {
        if (nullptr != _buckets) {
            deallocate_buckets(_buckets, _bucket_size);
            delete[] _locks;
            _buckets = nullptr;
            _locks = nullptr;
            _bucket_size = 0;
        }
    }

    size_t get_bucket_idx(const K &key) const {
        return _hasher(key) % _bucket_size;
    }

    std::function<bool(NodePtr)> _intrusive_put_preprocess;
    std::function<void(NodePtr, bool)> _intrusive_get_postprocess;
    std::function<void(NodePtr, bool)> _intrusive_put_postprocess;
    std::function<void(NodePtr, NodePtr, bool)> _intrusive_exchange_postprocess;
    std::function<void(NodePtr, bool)> _intrusive_erase_postprocess;
};

}

#endif //RPC_COMMON_CONCURRENT_HASH_MAP_H
