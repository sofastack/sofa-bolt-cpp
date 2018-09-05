// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/8.
//

#ifndef RPC_INCLUDE_CHANNEL_H
#define RPC_INCLUDE_CHANNEL_H

#include <pthread.h>
#include <vector>
#include <memory>
#include "protocol/protocol_define.h"
#include "tcp/endpoint.h"
#include "common/common_defines.h"

namespace antflash {

enum class EConnectionType {
    /**
     * Single connection to server, try to hold the connection by heartbeat if protocol
     * supported, default connection type.
     */
    CONNECTION_TYPE_SINGLE,
    /**
     * Pooled connection to server, each thread holding one active connection is recommended.
     * If connections reach maximum limits, some threads will share connection and sending
     * data will be blocked.
     */
    CONNECTION_TYPE_POOLED,
    /**
     * Single connection to server, try reconnect to endpoint every single session.
     */
    CONNECTION_TYPE_SHORT
};

struct ChannelOptions {
    //Constructions
    ChannelOptions();
    ChannelOptions(const ChannelOptions&);
    ChannelOptions& operator=(const ChannelOptions&);

    /**
     * Connection timeout, -1 means waiting for connection permanently.
     */
    int32_t connect_timeout_ms;
    /**
     * One session timeout, include sending and receiving data, -1 means waiting permanently.
     */
    int32_t timeout_ms;
    /**
     * Max retry time, only retry when write/read error, if read timeout, no retry.
     */
    int32_t max_retry;
    /**
     * socket pool size, set in CONNECTION_TYPE_POOLED case.
     */
    int32_t pool_size;
    /**
     * Protocol type, only one kind of protocol can be hold by each channel
     */
    EProtocolType protocol;
    /**
     * Type of connection to server. CONNECTION_TYPE_SINGLE as default connection type
     */
    EConnectionType connection_type;
};

class Socket;
class SocketPool;
class LifeCycleLock;
struct SubChannel;

/**
 * Channel for RPC, with RPC remote information, connection type, protocol type and so on.
 * Channel should be init before any Session use this channel, after init success, Session
 * in varied threads could use one single channel parallel.
 */
class Channel {
friend class Session;
friend class ChannelUnitTest;
public:
    Channel() : _status(RPC_STATUS_INIT) {}
    ~Channel();

    /**
     * Try connecting channel to a single server whose address is given by @address
     * non-thread-safe, make sure init channel before using it by multiple threads.
     *
     * @param address: endpoint's address.
     * @param options: channel options.
     * @return
     */
    bool init(const EndPoint& address, const ChannelOptions* options);
    /**
     *
     * @param address: endpoint's address.
     * @param options: channel options.
     * @return
     */
    bool init(const char* address, const ChannelOptions* options);

    /**
     * get a copy string of the endpoint address of channel
     * @return
     */
    inline std::string getAddress() const {
        return _address.ipToStr();
    }

    /****** For Unit Test Begin ******/
    ERpcStatus status() const {
        return _status;
    }
    const ChannelOptions& option() const {
        return _options;
    }
    /****** For Unit Test End ******/
private:
    bool getSocket(std::shared_ptr<Socket>& socket);
    bool getSocketInternal(std::shared_ptr<Socket>& socket);
    bool getSubSocketInternal(std::shared_ptr<Socket>& socket);
    bool tryConnect(std::shared_ptr<Socket>& socket);

    static void deleteThreadLocalSubChannel(void* arg);

    EndPoint _address;

    std::shared_ptr<Socket> _socket;
    std::vector<std::shared_ptr<SubChannel>> _sub_channels;

    std::shared_ptr<LifeCycleLock> _lock;

    const Protocol* _protocol;
    ChannelOptions _options;

    ERpcStatus _status;

    //As thread_local in C++ could only be static variable, use pthread_key_t instead
    pthread_key_t _local_sub_channel_idx;
};

}

#endif //RPC_INCLUDE_CHANNEL_H
