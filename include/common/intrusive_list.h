// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/6/21.
// https://lark.alipay.com/rtid/ghwc6e/zlwaai
//

#ifndef RPC_COMMON_INTRUSIVE_LIST_H
#define RPC_COMMON_INTRUSIVE_LIST_H

#include <atomic>
#include <string>

namespace antflash {

template <typename T>
struct Node {
    T value;
    std::atomic<Node<T>*> prev;
    std::atomic<Node<T>*> next;

    Node() : prev(nullptr), next(nullptr) {}

    template<typename ...Args>
    Node(Args&& ...args) : value(std::forward<Args>(args)...),
                           prev(nullptr),
                           next(nullptr) {}
};

/**
 * intrusive double linked list for LRU cache, support concurrent pop/push front/back elements
 * and remove specific node from list concurrently, insert, find or iterators not supported.
 * @tparam T
 */
template <typename T, typename NodeType = Node<T>>
class IntrusiveList {
public:
    using NodePtr = NodeType*;
    using AtomicNodePtr = std::atomic<NodePtr>;

    IntrusiveList() : _header(new NodeType), _tail(new NodeType), _size(0) {
        _header->next.store(_tail, std::memory_order_relaxed);
        _tail->prev.store(_header, std::memory_order_relaxed);
    }

    ~IntrusiveList() {
        clear();
        delete _header;
        _header = nullptr;
        delete _tail;
        _tail = nullptr;
    }

    /**
     * clear all nodes in list, not thread compatible
     */
    void clear() {
        NodePtr cur = _header->next.load(std::memory_order_relaxed);
        while (cur != _tail) {
            NodePtr next = cur->next.load(std::memory_order_relaxed);
            _header->next.store(next, std::memory_order_relaxed);
            next->prev.store(_header, std::memory_order_relaxed);
            cur = next;
        }
        _size.store(0, std::memory_order_relaxed);
    }

    template<typename ...Args>
    NodePtr new_node(Args&& ...args) {
        NodePtr cur = new NodeType(std::forward<Args>(args)...);
        return cur;
    }

    void push_front(NodePtr cur) {
        if (nullptr == cur) {
            return;
        }
        while (true) {
            if (cur->prev.load(std::memory_order_acquire) != nullptr) {
                return;
            }
            NodePtr next = _header->next.load(std::memory_order_acquire);
            if (insert_once(_header, next, cur)) {
                _size.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        }
    }

    void push_back(NodePtr cur) {
        if (nullptr == cur) {
            return;
        }
        while (true) {
            if (cur->prev.load(std::memory_order_acquire) != nullptr) {
                return;
            }
            NodePtr prev = _tail->prev.load(std::memory_order_acquire);
            if (insert_once(prev, _tail, cur)) {
                _size.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        }
    }

    size_t size() const {
        return _size.load(std::memory_order_relaxed);
    }

    bool empty() {
        return size() == 0;
    }

    bool remove(NodePtr p) {
        while (true) {
            auto prev = p->prev.load(std::memory_order_acquire);
            auto next = p->next.load(std::memory_order_acquire);
            /**
             * other thread is deleting this node at same time, just return
             */
            if (prev == nullptr || next == nullptr) {
                return false;
            }
            if (remove_once(prev, next, p)) {
                _size.fetch_sub(1, std::memory_order_relaxed);
                return true;
            }
        }
    }

    NodePtr pop_front() {
        while (true) {
            NodePtr cur = _header->next.load(std::memory_order_acquire);
            if (cur == _tail) {
                return nullptr;
            }
            auto next = cur->next.load(std::memory_order_acquire);
            /**
             * other thread is deleting this node at same time, just try again
             */
            if (next == nullptr) {
                continue;
            }
            if (remove_once(_header, next, cur)) {
                _size.fetch_sub(1, std::memory_order_relaxed);
                return cur;
            }
        }
    }

    NodePtr pop_back() {
        while (true) {
            NodePtr cur = _tail->prev.load(std::memory_order_acquire);
            if (cur == _header) {
                return nullptr;
            }
            auto prev = cur->prev.load(std::memory_order_acquire);
            /**
             * other thread is deleting this node at same time, just try again
             */
            if (prev == nullptr) {
                continue;
            }
            if (remove_once(prev, _tail, cur)) {
                _size.fetch_sub(1, std::memory_order_relaxed);
                return cur;
            }
        }
    }

    //For unit test
    const NodePtr header() const {
        return _header;
    }
    //For unit test
    const NodePtr tail() const {
        return _tail;
    }

private:
    bool insert_once(NodePtr prev, NodePtr next, NodePtr cur) {
        /**
         * As only push_front and push_back supported, prev and next pointer would
         * never be null, no need to check if pointer is nullptr.
         */

        /** Insert Step1 : IS1
         * In following case this condition fails when:
         * 1, another thread is inserting data with same next in IS3 with success
         *    (A, B, C) in this thread before IS1, (A, B, D) in other thread after IS3
         * 2, another thread is removing data with same next in RS5 with success
         *    (A, C, E) in this thread before IS2, (A, C, B) in other thread after RS5
         * 3, another thread is removing data with same next in RS6 with success
         */
        if (next->prev.load(std::memory_order_acquire) != prev) {
            return false;
        }

        NodePtr expect = nullptr;
        if (!cur->prev.compare_exchange_strong(expect, prev, std::memory_order_acq_rel)) {
            //other thread is inserting same pointer now
            return false;
        }
        cur->next.store(next, std::memory_order_release);

        /**Insert Step2 : IS2
         * In following case this condition fails when:
         * 1, another thread is inserting data in IS2 with success
         *    (A, B, C) in this thread before IS2, (A, B, D) in other thread after IS2
         * 2, another thread is removing data in RS4 with success
         *    (B, C, E) in this thread before IS2, (A, C, B) in other thread after RS4
         * 3, another thread is removing data in RS5 with success
         *    (A, B, E) in this thread before IS2, (A, C, B) in other thread after RS5
         */
        expect = next;
        if (!prev->next.compare_exchange_strong(expect, cur, std::memory_order_acq_rel)) {
            //firstly reset next node and then prev node as we just CAS prev node.
            expect = next;
            cur->next.compare_exchange_strong(expect, nullptr, std::memory_order_acq_rel);
            expect = prev;
            cur->prev.compare_exchange_strong(expect, nullptr, std::memory_order_acq_rel);
            return false;
        }

        /**Insert Step3 : IS3
         * In following case this condition will wait until success when:
         * 1, another thread is deleting data in RS3 with success before RS5 fail
         *    (A, B, E) in this thread fail at IS3, (A, C, B) in other thread fail at RS5
         */
        while (true) {
            expect = prev;
            if (next->prev.compare_exchange_strong(expect, cur, std::memory_order_acq_rel)) {
                break;
            }
        }
        return true;
    }

    bool remove_once(NodePtr prev, NodePtr next, NodePtr cur) {
        /**Remove Step1 : RS1
         * In following case this condition fails when:
         * 1, another thread is deleting data in RS3 with success
         *    (A, C, B) in this thread before RS1, (B, D, C) in other thread after RS3
         * 2, another thread is inserting data before IS3
         *    (A, B, E) in this thread before RS1, (A, B, E) in other thread before IS3
         */
        if (next->prev.load(std::memory_order_acquire) != cur) {
            return false;
        }

        /**Remove Step2 : RS2
         * In following case this condition fails when:
         * 1, another thread is deleting data before RS6
         *    (A, C, B) in this thread before RS2, (B, D, C) in other thread before RS6
         * 2, another thread is deleting data after RS4
         *    (C, F, D) in this thread before RS2, (B, D, C) in other thread after RS4
         */
        if (prev->next.load(std::memory_order_acquire) != cur) {
            return false;
        }

        /**Remove Step3 : RS3
         * In following case this condition fails when:
         * 1, another thread is deleting data after RS6
         *    (B, D, C) in this thread before RS3, (A, C, B) in other thread after RS6
         * 2, another thread is deleting same data after RS3
         *    (A, C, B) in this thread before RS3, (A, C, B) in other thread after RS3
         */
        NodePtr expect = prev;
        if (!cur->prev.compare_exchange_strong(expect, nullptr, std::memory_order_acq_rel)) {
            return false;
        }

        /**Remove Step4 : RS4
         * In following case this condition fails when:
         * 1, another thread is inserting data in IS2 with success
         *    (A, C, B) in this thread before RS4, (B, C, E) in other thread after IS2
         */
        expect = next;
        if (!cur->next.compare_exchange_strong(expect, nullptr, std::memory_order_acq_rel)) {
            expect = nullptr;
            cur->prev.compare_exchange_strong(expect, prev, std::memory_order_acq_rel);
            return false;
        }

        /**Remove Step5 : RS5
         * In following case this condition fails when:
         * 1, another thread is inserting data in IS2 with success
         *    (A, C, B) in this thread before RS5, (A, B, E) in other thread after IS2
         * 2, another thread is deleting data in RS4 with success
         *    (B, D, C) in this thread before RS5, (A, C, B) in other thread after RS4
         */
        expect = cur;
        if (!prev->next.compare_exchange_strong(expect, next, std::memory_order_acq_rel)) {
            expect = nullptr;
            cur->prev.compare_exchange_strong(expect, prev, std::memory_order_acq_rel);
            expect = nullptr;
            cur->next.compare_exchange_strong(expect, next, std::memory_order_acq_rel);
            return false;
        }

        /**Remove Step6 : RS6
         * In following case this condition will wait until success when:
         * 1, another thread is deleting data before RS5 with success
         *    (B, D, C) in this thread before RS6, (A, C, B) in other thread before fail in RS5
         */
        while (true) {
            expect = cur;
            if (next->prev.compare_exchange_strong(expect, prev, std::memory_order_acq_rel)) {
                break;
            }
        }

        return true;
    }

    NodePtr _header;
    NodePtr _tail;
    std::atomic<size_t> _size;
};

}

#endif //RPC_COMMON_INTRUSIVE_LIST_H
