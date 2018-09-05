// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/2.
//

#ifndef RPC_TCP_SOCKET_H
#define RPC_TCP_SOCKET_H

#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <future>
#include "socket_base.h"
#include "common/common_defines.h"
#include "common/io_buffer.h"
#include "common/life_cycle_lock.h"
#include "common/lockfree_queue.h"

namespace antflash {

struct Protocol;
class Socket;
class ResponseBase;

struct SocketReadSession {
    enum ESocketReadStatus : int32_t {
        INIT = 0,
        SUCCESS,
        FAIL
    };
    //Request ID
    size_t request_id;

    //Request time
    size_t request_time;

    //Async session callback
    std::function<void(ESessionError, ResponseBase*)> callback;
    ResponseBase* response;
    size_t expire_time;
    size_t timer_task_id;

    //Sync Notify specific session data is ready or timeout
    std::promise<ESessionError> result;

    //Protocol that session used
    const Protocol* protocol;

    //life cycle and owner status
    LifeCycleLock owners;

    //read buffer
    IOBuffer read_buf;

    //other data info
    void* data;

    /**
     * To notify session and do post process when receive data or timeout.
     * As notify could be called by two thread:OnRead thread and Timeout thread,
     * and read session memory is always reclaimed by OnRead thread. We should
     * make sure timeout event always happen and release shared status in timeout
     * event. There are two scenarios:
     * 1, OnRead happens before timeout: OnRead upgrade owner success and go on
     *   dealing post process and timeout thread try upgrade fail, timeout thread
     *   just release shared status.
     * 2, OnRead happens after timeout: OnRead upgrade owner fail and timeout thread
     *   deals with post process and then release shared status.
     * For Sync case: No matter which thread notify waiting work thread, add shared
     * num before notify, and make sure memory not be reclaimed during work thread
     * doing post process.
     * As session memory is reclaimed in OnRead thread, so reclaim action and notify
     * wouldn't happen at same time in OnRead thread, we don't need to worry about
     * session memory segment in OnRead thread.
     * @param err session error code
     */
    inline bool notify(ESessionError err) {
        //If shared fail, it means the other thread holds upgrade
        if (!owners.tryShared()) {
            return false;
        }

        //If upgrade fail, it means the other thread holds upgrade
        bool notify_result = false;
        if (owners.tryUpgradeNonReEntrant()) {
            notify_result = true;
            //For async case
            if (callback) {
                postProcess(err);
                callback(err, response);
            } else {
                //In sync case, set promise and return, in this step
                //owners has two shared owner, one for timeout thread,
                //one for sync working thread.
                result.set_value(err);
                return notify_result;
            }
        }
        //Release sync shared status if sync fail or in async case
        owners.releaseShared();
        return notify_result;
    }

    /**
     * Post process when receive data or timeout. For some protocol like
     * bolt, read session may just analyse header of buffer, the whole
     * deserialize will be done in this method. The other protocol like
     * http, post process do nothing.
     * @param error
     */
    void postProcess(ESessionError& error);
};

class Socket : public std::enable_shared_from_this<Socket> {
friend class SocketManager;
public:
    Socket(const EndPoint& remote) :
            _remote(remote),
            _status(RPC_STATUS_INIT),
            _session_info(MAX_PARALLEL_SESSION_SIZE_ON_SOCKET),
            _read_additional_data(nullptr) {
        _session_map.reserve(MAX_PARALLEL_SESSION_SIZE_ON_SOCKET);
    }
    ~Socket();

    /**
     * return socket's file descriptor
     * @return file descriptor
     */
    int fd() const {
        return _fd.fd();
    }

    /**
     * return socket's related remote endpoint
     * @return endpoint
     */
    EndPoint getRemote() const {
        return _remote;
    }

    void setBindProtocol(const Protocol *protocol);
    const Protocol* getProtocol() const;

    bool connect(int32_t connect_timeout_ms = -1);
    void disconnect();
    bool active() {
        return _status.load(std::memory_order_relaxed) == RPC_STATUS_OK;
    }
    bool write(IOBuffer& buffer, int32_t timeout_ms);

    size_t get_last_active_time() const {
        return _last_active_time_us.load(std::memory_order_acquire);
    }

    bool prepareRead(SocketReadSession *val) {
        return _session_info.push(val);
    }

    inline bool tryShared() {
        return _sharers.tryShared();
    }

    inline void releaseShared() {
        return _sharers.releaseShared();
    }

    inline bool tryExclusive() {
        return _sharers.tryUpgrade()
               && _sharers.tryExclusive();
    }

    inline void exclusive() {
        _sharers.exclusive();
    }

    inline void upgrade() {
        _sharers.upgrade();
    }

    inline void releaseExclusive() {
        _sharers.releaseExclusive();
    }

    inline void setStatus(ERpcStatus fail) {
        _status.store(fail, std::memory_order_relaxed);
    }

private:
    void onRead();
    void tryReclaimSessionMap();
    ssize_t cutIntoMessage();

    struct SocketConnection {
        int32_t timeout_ms;
        std::function<void()> on_connection;
    };

    EndPoint _remote;

    base::FdGuard _fd;
    SocketConnection _connection;
    std::atomic<ERpcStatus> _status;

    std::function<void()> _on_read;
    const Protocol* _protocol;
    IOBuffer _read_buf;

    MPSCQueue<SocketReadSession*> _session_info;
    std::unordered_map<size_t, SocketReadSession*> _session_map;

    std::atomic<size_t> _last_active_time_us;
    std::recursive_mutex _write_mtx;
    LifeCycleLock _sharers;

    //TODO use more effective and proper way to transfer data
    void* _read_additional_data;
};

}

#endif //RPC_TCP_SOCKET_H
