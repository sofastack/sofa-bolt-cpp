// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/13.
//

#ifndef RPC_TCP_SOCKET_MANAGER_H
#define RPC_TCP_SOCKET_MANAGER_H

#include <functional>
#include <thread>
#include <list>
#include <mutex>
#include <condition_variable>
#include "socket.h"

namespace antflash {

class SocketManager {
public:
    static SocketManager& getInstance() {
        static SocketManager s_socket_mgr;
        return s_socket_mgr;
    }

    bool init();
    void destroy();
    void addWatch(std::shared_ptr<Socket>& socket);

private:
    SocketManager() : _exit(false),
                      _reclaim_counter(0) {}
    ~SocketManager() {
        destroy();
    }

    void watchConnections();

    std::atomic<bool> _exit;
    std::unique_ptr<std::thread> _thread;
    std::list<std::shared_ptr<Socket>> _list;
    std::list<std::shared_ptr<Socket>> _reclaim_list;
    base::FdGuard _reclaim_fd[2];
    std::vector<std::function<void()>> _on_reclaim;
    std::mutex _reclaim_mtx;
    std::condition_variable _reclaim_notify;
    std::vector<bool> _reclaim_notify_flag;
    size_t _reclaim_counter;
    std::mutex _mtx;
};

}

#endif //RPC_TCP_SOCKET_MANAGER_H
