// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/8.
//

#include "channel/channel.h"
#include <limits>
#include <random>
#include "common/common_defines.h"
#include "common/life_cycle_lock.h"
#include "common/log.h"
#include "tcp/socket.h"

namespace antflash {

struct SubChannel {
    Channel channel;
    std::atomic<size_t> shared_num;
    std::atomic<size_t> is_active;
};

struct ThreadLocalSubChannel {
    ssize_t idx;
    const Channel* owner;
};

void Channel::deleteThreadLocalSubChannel(void* arg) {
    auto sub_channel = static_cast<ThreadLocalSubChannel*>(arg);
    if (sub_channel->idx >= 0) {
        sub_channel->owner
                ->_sub_channels[sub_channel->idx]
                ->shared_num.fetch_sub(1);
    }
    delete sub_channel;
}

ChannelOptions::ChannelOptions() :
        connect_timeout_ms(SOCKET_CONNECT_TIMEOUT_MS),
        timeout_ms(SOCKET_TIMEOUT_MS),
        max_retry(SOCKET_MAX_RETRY),
        pool_size(std::thread::hardware_concurrency()),
        protocol(EProtocolType::PROTOCOL_BOLT),
        connection_type(EConnectionType::CONNECTION_TYPE_SINGLE) {
}

ChannelOptions::ChannelOptions(const ChannelOptions& right) :
        connect_timeout_ms(right.connect_timeout_ms),
        timeout_ms(right.timeout_ms),
        max_retry(right.max_retry),
        pool_size(right.pool_size),
        protocol(right.protocol),
        connection_type(right.connection_type) {
}

ChannelOptions& ChannelOptions::operator=(const ChannelOptions& right) {
    if (&right != this) {
        connect_timeout_ms = right.connect_timeout_ms;
        timeout_ms = right.timeout_ms;
        max_retry = right.max_retry;
        pool_size = right.pool_size;
        protocol = right.protocol;
        connection_type = right.connection_type;
    }

    return *this;
}

Channel::~Channel() {
    if (_options.connection_type ==
        EConnectionType::CONNECTION_TYPE_POOLED) {
        pthread_key_delete(_local_sub_channel_idx);
    } else {
        if (_socket) {
            //Surrender the ownership so that socket manager can reclaim this socket.
            _socket->releaseExclusive();
        }
    }
}

bool Channel::init(const EndPoint& address,
                   const ChannelOptions *options) {
    if (options) {
        _options = *options;
    }
    _address = address;
    _protocol = getProtocol(_options.protocol);
    _lock.reset(new LifeCycleLock);

    bool ret = false;
    if (_options.connection_type == EConnectionType::CONNECTION_TYPE_POOLED) {
        pthread_key_create(&_local_sub_channel_idx, deleteThreadLocalSubChannel);

        if (_options.pool_size <= 0) {
            LOG_ERROR("connection pool size should be large than 0.");
            return false;
        }

        _sub_channels.resize(_options.pool_size);
        for (auto& sub_channel : _sub_channels) {
            sub_channel.reset(new SubChannel);
            sub_channel->shared_num.store(0, std::memory_order_relaxed);
            sub_channel->is_active.store(true, std::memory_order_relaxed);
            sub_channel->channel._address = _address;
            sub_channel->channel._protocol = _protocol;
            sub_channel->channel._lock.reset(new LifeCycleLock);
            ret = sub_channel->channel.tryConnect(
                    sub_channel->channel._socket);
            if (!ret) {
                break;
            }
        }
        if (!ret) {
            _sub_channels.clear();
        }
    } else if (_options.connection_type == EConnectionType::CONNECTION_TYPE_SHORT) {
        //For short connection, not connect socket here but when @getSocket
        ret = true;
    } else {
        ret = tryConnect(_socket);
    }

    if (!ret) {
        _status = RPC_STATUS_SOCKET_CONNECT_FAIL;
    } else {
        _status = RPC_STATUS_OK;
    }

    return ret;
}

bool Channel::init(const char* address,
                   const ChannelOptions* options) {
    EndPoint addr;
    if (!addr.parseFromString(address)) {
        LOG_ERROR("parse address fail: {}", address);
        return false;
    }

    return init(addr, options);
}

bool Channel::getSocket(std::shared_ptr<antflash::Socket> &socket) {
    if (_options.connection_type == EConnectionType::CONNECTION_TYPE_POOLED) {
        return getSubSocketInternal(socket);
    } else {
        return getSocketInternal(socket);
    }
}

bool Channel::getSocketInternal(std::shared_ptr<Socket>& socket) {
    //RAII release channel's shared status automatically
    {
        LifeCycleShareGuard guard(*_lock);
        //try sharing channel
        if (!guard.shared()) {
            LOG_INFO("some other thread is trying to create new socket now");
            return false;
        }

        //Set socket status to INIT every time
        if (_options.connection_type == EConnectionType::CONNECTION_TYPE_SHORT) {
            if (_socket) {
                _socket->setStatus(ERpcStatus::RPC_STATUS_INIT);
            }
        }

        //Channel always hold the ownership of its creating sockets
        //If  socket is not active anymore, surrender the ownership
        // so that socket manager can reclaim this socket.
        if (_socket && _socket->active()) {
            //As channel life cycle lock protect this assignment,
            //no race condition here, as socket is a shared point
            //and no more assignment in session object, even though
            //thread A is using socket in session function, and thread
            //B surrender the socket to socket manager, the socket
            //shared object deconstruction is still thread safe.
            socket = _socket;
            return true;
        }

        LOG_INFO("socket is not active anymore");

        //If socket connect fail, surrender the ownership
        //Firstly try hold channel for thread safety
        if (!_lock->tryUpgradeNonReEntrant()) {
            LOG_INFO("some other thread is trying to create new socket now");
            return false;
        }
    }

    //Waiting for other thread's shared channel release its shared status, blocking here
    _lock->exclusive();

    //Surrender the ownership so that socket manager can reclaim this socket.
    if (_socket) {
        _socket->releaseExclusive();
    }

    //Try create a new socket connection
    LOG_INFO("try reconnect to remote:{}", _address.ipToStr());
    bool ret = tryConnect(_socket);
    if (ret) {
        socket = _socket;
    } else {
        _status = RPC_STATUS_SOCKET_CONNECT_FAIL;
    }

    //release channel's exclusive status
    _lock->releaseExclusive();

    return ret;
}

bool Channel::tryConnect(std::shared_ptr<Socket>& socket) {
    LOG_DEBUG("try connect socket");
    socket.reset(new Socket(_address));
    //Hold the ownership of this socket to make sure not be reclaimed by socket manager
    //Always success here.
    socket->tryExclusive();
    socket->setBindProtocol(_protocol);
    bool ret = socket->connect(_options.connect_timeout_ms);

    if (!ret) {
        //If socket connect fail, surrender the ownership
        socket->releaseExclusive();
        socket.reset();
    }

    return ret;
}

bool Channel::getSubSocketInternal(std::shared_ptr<Socket> &socket) {
    LOG_DEBUG("try get sub socket");
    //Get thread local sub channel index
    ThreadLocalSubChannel* sub_channel =
            (ThreadLocalSubChannel*)pthread_getspecific(_local_sub_channel_idx);
    if (nullptr == sub_channel) {
        sub_channel = new ThreadLocalSubChannel;
        sub_channel->idx = -1;
        sub_channel->owner = this;
        pthread_setspecific(_local_sub_channel_idx, sub_channel);
    }

    //Check if this sub channel is active
    if (sub_channel->idx >= 0) {
        auto& channel = _sub_channels[sub_channel->idx]->channel;
        if (channel.getSocketInternal(socket)) {
            LOG_DEBUG("get idx {} from pool success.", sub_channel->idx);
            return true;
        }
        _sub_channels[sub_channel->idx]->is_active.
                store(false, std::memory_order_release);
        _sub_channels[sub_channel->idx]->shared_num.
                fetch_sub(1, std::memory_order_relaxed);
    }

    LOG_DEBUG("try get channel from pool.");

    while (true) {
        //Try get an idle sub channel from pool
        ssize_t most_idle_sub_channel = 0;
        size_t minus_shared_num = std::numeric_limits<size_t>::max();
        for (size_t i = 0; i < _sub_channels.size(); ++i) {
            if (sub_channel->idx == (ssize_t) i) {
                continue;
            }

            if (!_sub_channels[i]->is_active
                    .load(std::memory_order_acquire)) {
                continue;
            }

            size_t shared_num = _sub_channels[i]->shared_num.
                    load(std::memory_order_relaxed);
            if (shared_num < minus_shared_num) {
                minus_shared_num = shared_num;
                most_idle_sub_channel = (ssize_t)i;
            }
        }

        //All channel in pool is not active then random one
        if (minus_shared_num == std::numeric_limits<size_t>::max()) {
            std::random_device rd;
            most_idle_sub_channel = rd() % _sub_channels.size();
            minus_shared_num = _sub_channels[most_idle_sub_channel]->shared_num.
                    load(std::memory_order_relaxed);
        }

        sub_channel->idx = most_idle_sub_channel;
        if (_sub_channels[sub_channel->idx]->shared_num.
                compare_exchange_strong(minus_shared_num, minus_shared_num + 1,
                                        std::memory_order_acq_rel)) {
            break;
        }

    }
    LOG_DEBUG("try get channel from pool, idx:{}.", sub_channel->idx);

    if (sub_channel->idx >= 0) {
        //Check if this sub channel is active
        auto &channel = _sub_channels[sub_channel->idx]->channel;
        if (channel.getSocketInternal(socket)) {
            LOG_DEBUG("get idx {} from pool success.", sub_channel->idx);
            _sub_channels[sub_channel->idx]->is_active.
                    store(true, std::memory_order_release);
            return true;
        }

        _sub_channels[sub_channel->idx]->is_active.
                store(false, std::memory_order_release);
        _sub_channels[sub_channel->idx]->shared_num.
                fetch_sub(1, std::memory_order_release);
        sub_channel->idx = -1;
    }

    LOG_ERROR("try get channel from pool fail.");
    return false;
}

}
