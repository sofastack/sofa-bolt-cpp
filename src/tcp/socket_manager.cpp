// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/13.
//

#include "socket_manager.h"
#include <future>
#include <poll.h>
#include "common/utils.h"
#include "session/session.h"
#include "schedule/schedule.h"
#include "common/log.h"

namespace antflash {

bool SocketManager::init() {
    _exit.store(false, std::memory_order_release);

    int reclaim_fd[2];
    reclaim_fd[0] = -1;
    reclaim_fd[1] = -1;
    if (pipe(reclaim_fd) != 0) {
        return false;
    }
    _reclaim_fd[0] = reclaim_fd[0];
    _reclaim_fd[1] = reclaim_fd[1];

    auto size = Schedule::getInstance().scheduleThreadSize();
    _on_reclaim.reserve(size);
    _reclaim_notify_flag.resize(size, false);
    for (size_t i = 0; i < size; ++i) {
        _on_reclaim.emplace_back([this, i]() {
            _reclaim_notify_flag[i] = true;
            Schedule::getInstance().removeSchedule(
                    _reclaim_fd[1].fd(), POLLOUT, i);
            std::lock_guard<std::mutex> guard(_reclaim_mtx);
            _reclaim_notify.notify_one();
        });
    }


    _thread.reset(new std::thread([this](){
        while (!_exit.load(std::memory_order_acquire)) {
            watchConnections();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }));

    return true;
}

void SocketManager::destroy() {
    _exit.store(true, std::memory_order_release);
    if (_thread && _thread->joinable()) {
        _thread->join();
        _thread.reset();
    }

    //reclaim all socket's memory
    for (auto& socket : _list) {
        //If channel still holds exclusive when socket manager destroy,
        //which means channel is still alive when whole process is
        //going to shut down, in this case, we granted that no more
        //socket request will be sent, and related socket will be
        // destroyed in channel.
        if (socket->_sharers.tryUpgrade()) {
            socket->_sharers.exclusive();
        }
        //Just push to reclaim list
        _reclaim_list.push_back(socket);
    }
    _list.clear();

    //As socket manager is destroyed after schedule manager, clear reclaim list directly
    for (auto socket : _reclaim_list) {
       socket->_on_read = std::function<void()>();
       LOG_DEBUG("reset socket:{}", socket->fd());
    }
    _reclaim_list.clear();
}

void SocketManager::addWatch(std::shared_ptr<Socket>& socket) {
    std::lock_guard<std::mutex> lock_guard(_mtx);
    _list.push_back(socket);
}

void SocketManager::watchConnections() {
    std::vector<std::shared_ptr<Socket>> sockets;
    //1. Collect sockets to be reclaimed or to be watched
    {
        std::lock_guard<std::mutex> lock_guard(_mtx);
        sockets.reserve(_list.size());
        for (auto itr = _list.begin(); itr != _list.end();) {
            //As channel always exclusive it's socket when socket is active
            //If socket's exclusive status can be catch in socket manager,
            //it means this socket needs to be reclaimed.
            if ((*itr)->tryExclusive()) {
                LOG_INFO("socket[{}] is going to be reclaimed.", (*itr)->fd());
                //Disconnect socket just remove OnRead handler, we can not reclaim this
                // socket directly after disconnect as schedule manager may still call
                // this socket's OnRead event in its loop before it receive schedule
                // remove message, we could only do it in next loop.
                (*itr)->disconnect();
                _reclaim_list.emplace_back(*itr);
                itr = _list.erase(itr);
            } else {
                sockets.emplace_back(*itr);
                ++itr;
            }
        }
    }

    //2. Try to reclaim socket
    if (!_reclaim_list.empty()) {
        size_t schedule_size = Schedule::getInstance().scheduleThreadSize();
        size_t cur_idx = _reclaim_counter++ % schedule_size;
        _reclaim_notify_flag[cur_idx] = false;

        Schedule::getInstance().addSchedule(
                _reclaim_fd[1].fd(), POLLOUT,
                _on_reclaim[cur_idx], cur_idx);

        std::unique_lock<std::mutex> lock(_reclaim_mtx);
        auto status = _reclaim_notify.wait_for(
                lock, std::chrono::milliseconds(500),
                [this, cur_idx]() {
                    return _reclaim_notify_flag[cur_idx];
                });

        //Receiving notify successfully from schedule must happen in next loop as
        //adding schedule of reclaim is after removing schedule of sockets.
        //And in this case, sockets can be reclaimed safety.
        if (status) {
            for (auto itr = _reclaim_list.begin(); itr != _reclaim_list.end();) {
                if (((*itr)->fd() % schedule_size) == cur_idx) {
                    //release socket shared_from_this so that memory can be reclaimed
                    (*itr)->_on_read = std::function<void()>();
                    LOG_DEBUG("reset socket:{}", (*itr)->fd());
                    itr = _reclaim_list.erase(itr);
                } else {
                    ++itr;
                }
            }
        }
    }

    //3. send heartbeat to watch sockets
    for (auto& socket : sockets) {
        //If socket status is not active, and still in watch list, it means
        //this socket is not used by any other session yet, just skip it
        if (!socket->active()) {
            continue;
        }
        auto last_active_time = socket->get_last_active_time();
        if (SOCKET_MAX_IDLE_US <
                Utils::getHighPrecisionTimeStamp() - last_active_time) {
            if (socket->_protocol
                    && socket->_protocol->assemble_heartbeat_fn) {
                std::shared_ptr<RequestBase> request;
                std::shared_ptr<ResponseBase> response;
                if (socket->_protocol->assemble_heartbeat_fn(request, response)) {
                    Session ss;
                    ss.send(*request)
                            .to(socket)
                            .timeout(SOCKET_TIMEOUT_MS)
                            .receiveTo(*response)
                            .sync();
                    if (ss.failed()) {
                        socket->setStatus(RPC_STATUS_SOCKET_CONNECT_FAIL);
                    } else {
                        if (!socket->_protocol->parse_heartbeat_fn(response)) {
                            socket->setStatus(RPC_STATUS_SOCKET_CONNECT_FAIL);
                        } else {
                            LOG_INFO("remote[{}] heartbeat success",
                                          socket->getRemote().ipToStr());
                        }
                    }
                }
            }
        }
    }
}

}
