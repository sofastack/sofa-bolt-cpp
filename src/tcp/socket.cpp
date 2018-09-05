// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/2.
//

#include "socket.h"
#include <errno.h>
#include <poll.h>
#include "session/session.h"
#include "socket_manager.h"
#include "socket_base.h"
#include "common/utils.h"
#include "common/macro.h"
#include "common/log.h"
#include "schedule/schedule.h"
#include "protocol/protocol_define.h"

namespace antflash {

void SocketReadSession::postProcess(ESessionError &error) {
    if (error == ESessionError::SESSION_OK) {
        if (nullptr != response) {
            if (EResParseResult::PARSE_OK !=
                protocol->parse_response_fn(
                        *response, read_buf, data)) {
                error = ESessionError::PARSE_RESPONSE_FAIL;
            }
        }
    }
}

Socket::~Socket() {
    SocketReadSession* session = nullptr;
    while (_session_info.pop(session)) {
        _session_map.emplace(session->request_id, session);
    }
    while (!_session_map.empty()) {
        tryReclaimSessionMap();
    }
}

void Socket::setBindProtocol(const Protocol *protocol) {
    _protocol = protocol;
}

const Protocol* Socket::getProtocol() const {
    return _protocol;
}

bool Socket::connect(int32_t connect_timeout_ms) {
    _fd = base::create_socket();
    base::prepare_socket(_fd);

    int ret = base::connect(_fd, _remote);
    if (ret != 0 && errno != EINPROGRESS) {
        LOG_ERROR("connect fail, error no: {}", errno);
        setStatus(RPC_STATUS_SOCKET_CONNECT_FAIL);
        return false;
    }

    std::shared_ptr<std::promise<bool>> wakeup(new std::promise<bool>);
    auto self(shared_from_this());
    _connection.timeout_ms = connect_timeout_ms;
    _connection.on_connection = [self, wakeup]() {
        if (wakeup) {
            Schedule::getInstance().removeSchedule(self->_fd.fd(), POLLOUT);
            try {
                wakeup->set_value(true);
            } catch (std::future_error& ex) {
                LOG_ERROR("set wakeup fail: {}", ex.what());
            }
        }
    };

    if (!Schedule::getInstance().addSchedule(
            _fd.fd(), POLLOUT, _connection.on_connection)) {
        LOG_ERROR("add on connection schedule fail!");
        setStatus(RPC_STATUS_SOCKET_CONNECT_FAIL);
        return false;
    }

    if (connect_timeout_ms > 0) {
        std::chrono::milliseconds span(connect_timeout_ms);
        if (wakeup->get_future().wait_for(span) == std::future_status::timeout) {
            setStatus(RPC_STATUS_SOCKET_CONNECT_TIMEOUT);
            return false;
        }
    } else {
        wakeup->get_future().wait();
    }

    //Release it as it holds 'self' shared pointer
    _connection.on_connection = std::function<void()>();

    auto connected = base::connected(_fd);
    if (connected) {
        _last_active_time_us.store(
                Utils::getHighPrecisionTimeStamp(),
                std::memory_order_release);

        auto self(shared_from_this());
        _on_read = [self](){
            self->onRead();
        };
        if (!Schedule::getInstance().addSchedule(
                _fd.fd(), POLLIN, _on_read)) {
            LOG_ERROR("add on read schedule fail!");
            setStatus(RPC_STATUS_SOCKET_CONNECT_FAIL);
            //Release it as it holds 'self' shared pointer
            _on_read = std::function<void()>();
            return false;
        }

        //Add this socket to socket manager
        SocketManager::getInstance().addWatch(self);
        setStatus(RPC_STATUS_OK);
    } else {
        setStatus(RPC_STATUS_SOCKET_CONNECT_FAIL);
    }

    return connected;
}

void Socket::disconnect() {
    //remove onRead event from schedule
    Schedule::getInstance().removeSchedule(_fd.fd(), POLLIN);
}

bool Socket::write(IOBuffer& buffer, int32_t timeout_ms) {
    //TODO only allow one writer without std::mutex for async write
    //zhenggu.xwt:当前同步请求优先使用连接池中线程本地的socket连接，锁大概率不会陷入内核
    std::lock_guard<std::recursive_mutex> lock(_write_mtx);

    if (timeout_ms < 0) {
        LOG_ERROR("has already timeout:{}.", timeout_ms);
        return false;
    }

    auto buffer_size = buffer.length();
    if (0 >= buffer_size) {
        LOG_ERROR("data to write is empty!");
        return false;
    }

    size_t cur_size = 0;
    while (cur_size < buffer_size) {
        //Socket fd may be closed when writing data, and SIGPIPE will be sent
        //currently we just ignore SIGPIPE signal
        ssize_t nw = buffer.cut_into_file_descriptor(_fd.fd(), buffer_size);
        if (nw < 0) {
            if (errno != EAGAIN) {
                LOG_ERROR("Fail to write into {}", _remote.ipToStr());
                setStatus(RPC_STATUS_SOCKET_WRITE_ERROR);
                return false;
            }
        }
        LOG_DEBUG("write {} to {}", nw, _remote.ipToStr());
        cur_size += nw;
    }

    return true;
}

void Socket::onRead() {
    if (UNLIKELY(nullptr == _protocol)) {
        LOG_ERROR("protocol not found.");
        setStatus(RPC_STATUS_SOCKET_READ_ERROR);
        return;
    }

    constexpr size_t MIN_ONCE_READ = 4096;
    //If last OnRead met some error, as socket may not be closed immediately,
    //OnRead event may still be triggered, in this case, just return and wait
    //for socket's closing.
    bool read_eof = !active();
    size_t total_nr = 0;

    while (!read_eof) {
        auto nr = _read_buf.append_from_file_descriptor(_fd.fd(), MIN_ONCE_READ);
        LOG_DEBUG("read receive size:{}", nr);

        if (0 == nr) {
            read_eof = true;
            //Server close this connection, and read buffer may not empty, do not return here
            setStatus(RPC_STATUS_SOCKET_CLOSED_BY_SERVER);
        } else if (0 > nr) {
            if (errno == EINTR) {
                continue;

#if defined(OS_MACOSX)
            //FIXME errnro readv ESRCH in MACOSX
            } else if (errno == EAGAIN || errno == ESRCH) {
#else
            } else if (errno == EAGAIN) {
#endif // defined(OS_MACOSX)
                break;
            }
            LOG_ERROR("fail to read from: {}, error no:{}", _remote.ipToStr(), errno);
            setStatus(RPC_STATUS_SOCKET_READ_ERROR);
            break;
        }

        total_nr += nr;

        ssize_t receive_request_id = 0;
        while (true) {
            receive_request_id = cutIntoMessage();
            //If 0: not enough data
            //If -1:error occur
            if (receive_request_id <= 0) {
                break;
            }
        }
        if (receive_request_id < 0) {
            break;
        }
    }

    //Try to release and reclaim unused session
    tryReclaimSessionMap();

    return;
}

ssize_t Socket::cutIntoMessage() {
    size_t receive_request_id = 0;
    size_t response_size = 0;
    LOG_DEBUG("read buffer size {}", _read_buf.size());
    if (_read_buf.size() == 0) {
        return 0;
    }
    auto ret = _protocol->parse_protocol_fn(
            _read_buf, response_size, receive_request_id, &_read_additional_data);
    if (ret == EResParseResult::PARSE_NOT_ENOUGH_DATA) {
        LOG_DEBUG("parse not enough data");
        return 0;
    } else if (ret == EResParseResult::PARSE_ERROR) {
        LOG_FATAL("parse data fail from: {}, close connection", _remote.ipToStr());
        setStatus(RPC_STATUS_SOCKET_READ_ERROR);
        return -1;
    }

    LOG_DEBUG("response data:{}", response_size);
    _last_active_time_us.store(Utils::getHighPrecisionTimeStamp(),
                               std::memory_order_release);

    size_t on_read_begin_time = Utils::getHighPrecisionTimeStamp();
    //Set session info to session map for reflect corresponding session
    SocketReadSession* session = nullptr;
    while (_session_info.pop(session)) {
        //If session created time is later than onRead time,
        //it means that session is inserted after onRead occur,
        //this session has nothing to do with this onRead.
        LOG_DEBUG("OnRead prepare:{}", session->request_id);
        auto p = _session_map.emplace(session->request_id, session);
        if (!p.second) {
            //Duplicated request id, impossible in normal case
            LOG_FATAL("IMPOSSIBLE CASE!!!!!MEMORY LEAK!!!!");
        }
        if (session->request_time >= on_read_begin_time) {
            break;
        }
    }

    decltype(_session_map)::iterator itr;
    if (_protocol->type == EProtocolType::PROTOCOL_HTTP) {
        itr = _session_map.begin();

    } else {
        //find correspond session and send sync notice
        itr = _session_map.find(receive_request_id);
    }
    if (itr == _session_map.end() || nullptr == itr->second) {
        LOG_WARN("request id {} not found in any session.", receive_request_id);
        _read_buf.pop_front(response_size);
        return receive_request_id;
    }

    session = itr->second;
    _read_buf.cut(&session->read_buf, response_size);
    session->data = _read_additional_data;
    _read_additional_data = nullptr;

    session->notify(ESessionError::SESSION_OK);

    return receive_request_id;
}

void Socket::tryReclaimSessionMap() {
    for (auto itr = _session_map.begin(); itr != _session_map.end();) {
        if (nullptr == itr->second) {
            itr = _session_map.erase(itr);
        } else {
            SocketReadSession* session = itr->second;
            //Session upgrade status will be set by OnRead thread or timeout thread
            //when @SocketReadSession::notify is called, and here we just wait for
            // session sync method and time out thread release shared and then reclaim it
            if (session->owners.tryExclusive()) {
                LOG_DEBUG("Reclaim session:{}", session->request_id);
                delete session;
                itr = _session_map.erase(itr);
            } else {
                ++itr;
            }
        }
    }
}

}
