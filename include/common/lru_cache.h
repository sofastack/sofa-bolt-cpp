// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/5/21.
//

#ifndef RPC_COMMON_LRU_CACHE_H
#define RPC_COMMON_LRU_CACHE_H

#include "common/intrusive_list.h"
#include "common/concurrent_hash_map.h"

namespace antflash {

template <typename K, typename V>
struct LRUNode : public Node<V>
{
    K key;
    LRUNode* hash_next;
    size_t used_mem;

    bool is_intrusive() const {
        return true;
    }

    template <typename ...Args>
    LRUNode(const K& key, size_t mem, Args&& ...args) :
            Node<V>(std::forward<Args>(args)...),
            key(key), hash_next(nullptr), used_mem(mem) {}
};

template <typename K, typename V>
class LRUCache : public ConcurrentHashMap<K, V, LRUNode<K,V>> {
public:
    using NodePtr = typename ConcurrentHashMap<K, V, LRUNode<K,V>>::NodePtr;
    using LRUNodePtr = LRUNode<K, V>*;

    LRUCache(size_t bucket_hint, size_t max_mem_bytes) :
            ConcurrentHashMap<K, V, LRUNode<K,V>>(
                    bucket_hint,
                    [this](NodePtr pp) {
                        LRUNodePtr p = static_cast<LRUNodePtr>(pp);
                        if (p->used_mem > _mem_size) {
                            return false;
                        }

                        return true;
                    },
                    [this](NodePtr pp, bool ret) {
                        if (ret) {
                            _hit_query_times.fetch_add(1, std::memory_order_relaxed);
                            Node<V>* p = static_cast<Node<V>*>(pp);
                            if (_list.remove(p)) {
                                _list.push_front(p);
                            }
                        }
                    },
                    [this](NodePtr pp, bool ret) {
                        if (ret) {
                            LRUNodePtr p = static_cast<LRUNodePtr>(pp);

                            if (p->used_mem + _mem_size_used.load(
                                    std::memory_order_acquire) > _mem_size) {
                                release_nodes(p->used_mem, p->key);
                            }

                            _mem_size_used.fetch_add(p->used_mem, std::memory_order_release);
                        }
                    },
                    [this](NodePtr new_node, NodePtr ori_node, bool ret) {
                        LRUNodePtr cur = static_cast<LRUNodePtr>(new_node);
                        if (ret) {
                            LRUNodePtr ori = static_cast<LRUNodePtr>(ori_node);
                            _mem_size_used.fetch_add(
                                    cur->used_mem - ori->used_mem, std::memory_order_release);
                        } else {
                            _mem_size_used.fetch_add(
                                    cur->used_mem, std::memory_order_release);
                        }

                        size_t mem_used = _mem_size_used.load(std::memory_order_acquire);
                        if (mem_used > _mem_size) {
                            release_nodes(mem_used - _mem_size, cur->key);
                        }
                    },
                    [this](NodePtr pp, bool ret) {
                        if (ret) {
                            LRUNodePtr p = static_cast<LRUNodePtr>(pp);
                            Node<V>* n = static_cast<Node<V>*>(p);
                            if (_list.remove(n)) {
                                delete p;
                            }
                            _mem_size_used.fetch_sub(p->used_mem, std::memory_order_release);
                        }
                    }
            ),
            _mem_size(max_mem_bytes),
            _mem_size_used(0),
            _hit_query_times(0),
            _total_query_times(0) {
    }

    ~LRUCache() {
        this->destroy();
        while (true) {
            auto p = _list.pop_back();
            if (p == nullptr) {
                break;
            }
            delete p;
        }
    }

    bool get(const K& key, V& value) {
        _total_query_times.fetch_add(1, std::memory_order_relaxed);
        return this->intrusive_get(key, value);
    }

    template <typename ...Args>
    bool put(const K& key, size_t mem, Args&& ...args) {
        LRUNodePtr p = new LRUNode<K, V>(key, mem, std::forward<Args>(args)...);
        if (!this->intrusive_put(key, p)) {
            delete p;
            return false;
        }

        Node<V>* pp = static_cast<Node<V>*>(p);
        _list.push_front(pp);

        return true;
    }

    template <typename ...Args>
    bool exchange(const K& key, size_t mem, Args&& ...args) {
        LRUNodePtr p = new LRUNode<K, V>(key, mem, std::forward<Args>(args)...);
        LRUNodePtr ori = p;
        bool ret = this->intrusive_exchange(key, p);

        //如果交换成功则更新链表，p为交换后的旧指针
        if (ret) {
            Node<V>* pp = static_cast<Node<V>*>(p);
            if (_list.remove(pp)) {
                delete p;
            }
        }

        Node<V>* pp = static_cast<Node<V>*>(ori);
        _list.push_front(pp);

        return ret;
    }

    void erase(const K& key) {
        this->intrusive_erase(key);
    }

    size_t get_hit_query_times() const {
        return _hit_query_times.load(std::memory_order_relaxed);
    }

    size_t get_total_query_times() const {
        return _total_query_times.load(std::memory_order_relaxed);
    }

    size_t get_mem_size_used() const {
        return _mem_size_used.load(std::memory_order_relaxed);
    }

private:
    void release_nodes(size_t mem, const K& locked_bucket_key) {
        while (mem > 0) {
            Node<V> *pp = _list.pop_back();
            if (pp == nullptr) {
                break;
            }
            LRUNodePtr p = static_cast<LRUNodePtr>(pp);

            if (this->get_bucket_idx(locked_bucket_key)
                    == this->get_bucket_idx(p->key)) {
                this->erase_nolock(p->key);
                _mem_size_used.fetch_sub(p->used_mem, std::memory_order_release);
            } else {
                //If key to removed is not in the same bucket with locked bucket,
                //try erase key with lock, and then try to remove this p from list
                //one more time to make sure that p is succeed removing from list.
                this->erase(p->key);
                _list.remove(p);
            }

            mem -= p->used_mem;
            delete p;
        }
    }

    size_t _mem_size;
    std::atomic<size_t> _mem_size_used;
    std::atomic<size_t> _hit_query_times;
    std::atomic<size_t> _total_query_times;

    IntrusiveList<V> _list;
};

}

#endif //RPC_COMMON_LRU_CACHE_H
