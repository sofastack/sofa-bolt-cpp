//
// Created by zhenggu.xwt on 18/6/21.
//

#include <common/intrusive_list.h>
#include <gtest/gtest.h>
#include <thread>
#include <mutex>
#include <map>
#include <random>
#include <list>

using namespace antflash;

constexpr size_t MAX_SIZE = 102400;

TEST(IntrusiveListTest, singleThread) {
    IntrusiveList<int> list;

    ASSERT_TRUE(list.empty());
    auto node1 = list.new_node(1);
    auto node2 = list.new_node(2);
    list.push_front(nullptr);
    list.push_front(node1);
    list.push_back(nullptr);
    list.push_back(node2);

    ASSERT_EQ(list.size(), 2UL);
    auto p = list.header()->next.load(std::memory_order_relaxed);
    ASSERT_EQ(p, node1);
    p = p->next.load(std::memory_order_relaxed);
    ASSERT_EQ(p, node2);
    p = p->next.load(std::memory_order_relaxed);
    ASSERT_EQ(p, list.tail());

    p = list.pop_back();
    ASSERT_EQ(p, node2);
    ASSERT_EQ(list.size(), 1UL);
    p = list.pop_front();
    ASSERT_EQ(p, node1);
    ASSERT_EQ(list.size(), 0UL);

    list.push_front(node1);
    list.push_back(node1);
    ASSERT_EQ(list.size(), 1UL);
    p = list.header()->next.load(std::memory_order_relaxed);
    ASSERT_EQ(p, node1);
    p = p->next.load(std::memory_order_relaxed);
    ASSERT_EQ(p, list.tail());
    p = list.pop_front();
    ASSERT_EQ(p, node1);
    ASSERT_TRUE(list.empty());
    p = list.pop_front();
    ASSERT_EQ(p, nullptr);

    delete node1;
    delete node2;
}

TEST(IntrusiveListTest, multiThreadPush) {
    IntrusiveList<int> list;
    auto dest_time = std::chrono::system_clock::now()
                     + std::chrono::milliseconds(100);

    constexpr size_t data_size = MAX_SIZE;
    Node<int> data[data_size];
    for (size_t i = 0; i < data_size; ++i) {
        data[i].value = i;
        data[i].prev = nullptr;
        data[i].next = nullptr;
    }

    std::thread t1([&list, &dest_time, &data, data_size]() {
        std::this_thread::sleep_until(dest_time);

        for (size_t i = 0; i < data_size; ++i) {
            list.push_front(&data[i]);
        }
    });

    std::thread t2([&list, &dest_time, &data, data_size]() {
        std::random_device rd;
        for (size_t i = 0; i < data_size; ++i) {
            size_t rand = rd() % data_size;
            list.push_front(&data[rand]);
        }
    });

    std::thread t3([&list, &dest_time, &data, data_size]() {
        std::random_device rd;
        for (size_t i = 0; i < data_size; ++i) {
            size_t rand = rd() % data_size;
            list.push_front(&data[rand]);
        }
    });

    t1.join();
    t2.join();
    t3.join();

    const Node<int>* const header = list.header();
    const Node<int>* const tail  = list.tail();
    auto p = header->next.load();
    std::set<int> results;
    while (p != tail) {
        Node<int>* q = &data[p->value];
        ASSERT_EQ(p, q);
        results.insert(p->value);
        p = p->next.load();
    }
    ASSERT_EQ(list.size(), data_size);
    ASSERT_EQ(results.size(), data_size);
}

TEST(IntrusiveListTest, multiThreadPop) {
    IntrusiveList<int> list;

    constexpr size_t data_size = MAX_SIZE;
    Node<int> data[data_size];
    for (size_t i = 0; i < data_size; ++i) {
        data[i].value = i;
        data[i].prev = nullptr;
        data[i].next = nullptr;
    }

    for (size_t i = 0; i < data_size; ++i) {
        list.push_front(&data[i]);
    }

    ASSERT_EQ(list.size(), data_size);

    auto dest_time = std::chrono::system_clock::now()
                     + std::chrono::milliseconds(100);

    std::thread t1([&list, &dest_time, &data, data_size]() {
        std::this_thread::sleep_until(dest_time);

        for (size_t i = 0; i < data_size; ++i) {
            auto p = list.pop_back();
            if (p != nullptr) {
                ASSERT_EQ(p->next.load(), nullptr);
                ASSERT_EQ(p->prev.load(), nullptr);
            }
        }
    });

    std::thread t2([&list, &dest_time, &data, data_size]() {
        std::this_thread::sleep_until(dest_time);
        std::random_device rd;
        for (size_t i = 0; i < data_size; ++i) {
            size_t rand = rd() % data_size;
            list.remove(&data[rand]);
            ASSERT_EQ(data[rand].next.load(), nullptr);
            ASSERT_EQ(data[rand].prev.load(), nullptr);
        }
    });

    std::thread t3([&list, &dest_time, &data, data_size]() {
        std::this_thread::sleep_until(dest_time);
        std::random_device rd;
        for (size_t i = 0; i < data_size; ++i) {
            auto p = list.pop_front();
            if (p != nullptr) {
                ASSERT_EQ(p->next.load(), nullptr);
                ASSERT_EQ(p->prev.load(), nullptr);
            }
        }
    });

    t1.join();
    t2.join();
    t3.join();

    ASSERT_EQ(list.size(), 0UL);
    for (size_t i = 0; i < data_size; ++i) {
        auto p = &data[i];
        ASSERT_EQ(p->next.load(), nullptr);
        ASSERT_EQ(p->prev.load(), nullptr);
    }
}

TEST(IntrusiveListTest, multiThreadPopPush) {
    IntrusiveList<int> list;

    constexpr size_t data_size = MAX_SIZE;
    Node<int> data[data_size];
    for (size_t i = 0; i < data_size; ++i) {
        data[i].value = i;
        data[i].prev = nullptr;
        data[i].next = nullptr;
    }

    for (size_t i = 0; i < data_size; ++i) {
        list.push_front(&data[i]);
    }

    ASSERT_EQ(list.size(), data_size);

    auto dest_time = std::chrono::system_clock::now()
                     + std::chrono::milliseconds(100);

    std::thread t1([&list, &dest_time, &data, data_size]() {
        std::this_thread::sleep_until(dest_time);

        for (size_t i = 0; i < data_size; ++i) {
            auto p = list.pop_back();
            if (p != nullptr) {
                ASSERT_EQ(p->next.load(), nullptr);
                ASSERT_EQ(p->prev.load(), nullptr);
            }
            list.push_front(p);
        }
    });

    std::thread t2([&list, &dest_time, &data, data_size]() {
        std::this_thread::sleep_until(dest_time);

        auto p = list.pop_front();
        if (p != nullptr) {
            ASSERT_EQ(p->next.load(), nullptr);
            ASSERT_EQ(p->prev.load(), nullptr);
        }
        list.push_back(p);
    });

    t1.join();
    t2.join();

    const Node<int>* const header = list.header();
    const Node<int>* const tail  = list.tail();
    auto p = header->next.load();
    std::set<int> results;
    while (p != tail) {
        Node<int>* q = &data[p->value];
        ASSERT_EQ(p, q);
        results.insert(p->value);
        p = p->next.load();
    }
    ASSERT_EQ(list.size(), data_size);
    ASSERT_EQ(results.size(), data_size);

    list.clear();
    ASSERT_EQ(list.size(), 0UL);
}

template<class T>
struct LockNode {
    T value;
    LockNode* next;
    LockNode* prev;

    LockNode() : next(nullptr), prev(nullptr) {}
};

template<class T>
class LockIntrusiveList {
public:
    LockIntrusiveList() : _size(0) {
        _header.next = &_tail;
        _tail.prev = &_header;
    }

    void push_front(LockNode<T>* cur) {
        if (nullptr == cur) {
            return;
        }
        std::lock_guard<std::mutex> guard(_mtx);
        cur->next = _header.next;
        cur->prev = &_header;

        _header.next->prev = cur;
        _header.next = cur;
        ++_size;
    }

    void push_back(LockNode<T>* cur) {
        if (nullptr == cur) {
            return;
        }
        std::lock_guard<std::mutex> guard(_mtx);
        cur->next = &_tail;
        cur->prev = _tail.prev;

        _tail.prev->next = cur;
        _tail.prev = cur;
        ++_size;
    }

    size_t size() const {
        return _size;
    }

    bool empty() {
        return size() == 0;
    }

    LockNode<T>* pop_front() {
        std::lock_guard<std::mutex> guard(_mtx);
        if (_header.next == &_tail) {
            return nullptr;
        }
        auto p = _header.next;
        _header.next = p->next;
        p->next->prev = &_header;
        p->prev = nullptr;
        p->next = nullptr;
        return p;
    }

    LockNode<T>* pop_back() {
        std::lock_guard<std::mutex> guard(_mtx);
        if (_tail.prev == &_header) {
            return nullptr;
        }
        auto p = _tail.prev;
        _tail.prev = p->prev;
        p->prev->next = &_tail;
        p->prev = nullptr;
        p->next = nullptr;
        return p;
    }

private:
    LockNode<T> _header;
    LockNode<T> _tail;
    size_t _size;
    std::mutex _mtx;
};

TEST(IntrusiveListTest, multiThreadPopPushLock) {
    LockIntrusiveList<int> list;
    std::mutex mtx;

    constexpr size_t data_size = MAX_SIZE;
    LockNode<int> data[data_size];
    for (size_t i = 0; i < data_size; ++i) {
        data[i].value = i;
        data[i].prev = nullptr;
        data[i].next = nullptr;
    }

    for (size_t i = 0; i < data_size; ++i) {
        list.push_front(&data[i]);
    }

    ASSERT_EQ(list.size(), data_size);

    auto dest_time = std::chrono::system_clock::now()
                     + std::chrono::milliseconds(100);

    std::thread t1([&list, &dest_time, &data, data_size]() {
        std::this_thread::sleep_until(dest_time);

        for (size_t i = 0; i < data_size; ++i) {
            auto p = list.pop_back();
            if (p != nullptr) {
                ASSERT_EQ(p->next, nullptr);
                ASSERT_EQ(p->prev, nullptr);
            }
            list.push_front(p);
        }
    });

    std::thread t2([&list, &dest_time, &data, data_size]() {
        std::this_thread::sleep_until(dest_time);

        auto p = list.pop_front();
        if (p != nullptr) {
            ASSERT_EQ(p->next, nullptr);
            ASSERT_EQ(p->prev, nullptr);
        }
        list.push_back(p);
    });

    t1.join();
    t2.join();
}
