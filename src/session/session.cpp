// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/4.
//

#include "session/session.h"
#include <string>
#include <atomic>
#include <future>
#include <sstream>
#include "common/macro.h"
#include "common/utils.h"
#include "common/log.h"
#include "common/io_buffer.h"
#include "tcp/socket.h"
#include "schedule/schedule.h"

namespace antflash {

alignas(SIZE_OF_AVOID_FALSE_SHARING) std::atomic<size_t> s_session_id(1);

static std::string s_session_error_info[] = {
        "success",
        "protocol not found",
        "package request fail",
        "socket object lost",
        "socket is busy reading data",
        "write into remote fail",
        "read data from remote fail",
        "read data timeout",
        "parse response fail",
        "timer thread busy",
};

Session::~Session() {
}

void Session::reset() {
    _request = nullptr;
    _response = nullptr;
    _channel = nullptr;
    _error_code = ESessionError::SESSION_OK;
    _socket.reset();
}

const std::string& Session::getErrText() const {
    return s_session_error_info[static_cast<int>(_error_code)];
}

Session& Session::to(Channel& channel) {
    if (_timeout < 0) {
        _timeout = channel._options.timeout_ms;
    }
    _protocol = channel._protocol;
    if (_retry < 0) {
        _retry = channel._options.max_retry;
    }

    _channel = &channel;

    return *this;
}

Session& Session::to(std::shared_ptr<Socket>& socket) {
    _socket = socket;
    _protocol = socket->getProtocol();
    return *this;
}

Session& Session::sync() {
    sendInternalWithRetry(nullptr);
    return *this;
}

Session &Session::async(SessionAsyncCallback callback) {
    sendInternalWithRetry(&callback);
    if (failed()) {
        callback(_error_code, _response);
    }
    return *this;
}

void Session::sendInternalWithRetry(SessionAsyncCallback *callback) {
    size_t retry = _retry > 0 ? _retry : 1;
    for (size_t i = 0; i < retry; ++i) {
        _error_code = ESessionError::SESSION_OK;
        sendInternal(callback);
        //If session is ok or reading timeout, break the retry
        if (_error_code == ESessionError::SESSION_OK
            || _error_code == ESessionError::READ_TIMEOUT) {
            break;
        }
    }
}

void Session::sendInternal(SessionAsyncCallback* callback) {
    SocketReadSession* session_info = nullptr;

    do {
        if (nullptr == _protocol) {
            _error_code = ESessionError::PROTOCOL_NOT_FOUND;
            break;
        }

        if (nullptr != _channel) {
            if(!_channel->getSocket(_socket)) {
                LOG_ERROR("get channel socket fail.");
                _socket.reset();
            }
        }

        if (!_socket) {
            _error_code = ESessionError::SOCKET_LOST;
            break;
        }

        _begin_time_us = Utils::getHighPrecisionTimeStamp();
        size_t session_id = s_session_id.fetch_add(1, std::memory_order_relaxed);

        //1, package request data to io buffer
        IOBuffer write_buf;
        if (nullptr == _request ||
            !_protocol->assemble_request_fn(*_request, session_id, write_buf)) {
            _error_code = ESessionError::ASSEMBLE_REQUEST_FAIL;
            break;
        }

        //2, Init session info for later reading, and its life cycle is controlled in Socket
        session_info = new SocketReadSession;
        if (_protocol->converse_request_fn) {
            session_info->request_id = _protocol->converse_request_fn(session_id);
        } else {
            session_info->request_id = session_id;
        }
        session_info->request_time = _begin_time_us;
        session_info->protocol = _protocol;
        if (_timeout > 0) {
            session_info->expire_time = session_info->request_time + _timeout * 1000;
        } else {
            session_info->expire_time = std::numeric_limits<size_t>::max();
        }

        session_info->response = _response;
        if (nullptr != callback) {
            session_info->callback = std::move(*callback);
        }
        session_info->owners.tryShared();//for timeout thread, always success

        //3, Send session info to Socket, thread compatible
        if (!_socket->prepareRead(session_info)) {
            _error_code = ESessionError::SOCKET_BUSY;
            delete session_info;
            session_info = nullptr;
            break;
        }

        //4, Add timeout schedule
        session_info->timer_task_id = Schedule::getInstance().addTimeschdule(
                session_info->expire_time,
                [session_info]() {
                    auto error = ESessionError::READ_TIMEOUT;
                    if (session_info->notify(error)) {
                        LOG_WARN("request id {} is timeout", session_info->request_id);
                    }
                    LOG_DEBUG("release shared:{}", session_info->timer_task_id);
                    //release timeout thread shared status
                    session_info->owners.releaseShared();
                });
        LOG_DEBUG("add timeout:{}", session_info->timer_task_id);

        //If adding timeout fail, just release shared
        if (session_info->timer_task_id <= 0) {
            LOG_ERROR("add timeout fail, release shared:{}", session_info->timer_task_id);
            _error_code = ESessionError::TIMER_BUSY;
            session_info->owners.releaseShared();
            break;
        }

        Utils::Timer clock;
        //5, Write data to Socket's fd
        if (!_socket->write(write_buf, _timeout - clock.elapsed())) {
            _error_code = ESessionError::WRITE_FAIL;
            //If adding timeout fail, just remove timeout and release shared
            Schedule::getInstance().removeTimeschdule(session_info->timer_task_id);
            session_info->owners.releaseShared();
            break;
        }
        LOG_DEBUG("write data cost {} ms", clock.elapsed());

        //6, Sync waiting
        if (!session_info->callback) {
            _error_code = session_info->result.get_future().get();
            session_info->postProcess(_error_code);
            //Release sync shared status
            session_info->owners.releaseShared();
        }
    } while (0);
}

const std::string &Session::getErrText(ESessionError error) {
    return s_session_error_info[static_cast<int>(error)];
}

struct ParallelAsyncInfo {
    std::promise<bool> promise;
    std::vector<ESessionError> status;
    size_t received_size;
    size_t total_size;
};

PipelineSession& PipelineSession::sync() {
    _session._error_code = ESessionError::SESSION_OK;
    do {
        if (0 == _requests.size()) {
            break;
        }

        if (nullptr == _channel) {
            _session._error_code = ESessionError::PROTOCOL_NOT_FOUND;
            break;
        }

        auto info = std::make_shared<ParallelAsyncInfo>();
        info->status.resize(_requests.size(),
                            ESessionError::SESSION_OK);

        info->received_size = 0;
        info->total_size = _requests.size();
        for (size_t i = 0; i < _requests.size(); ++i) {
            auto& request = _requests[i];
            auto& response = _responses[i];

            _session.reset();
            _session.send(*request);
            _session.to(*_channel);
            _session.receiveTo(*response);
            _session.async([info, i](
                    ESessionError err, ResponseBase* rsp){
                info->status[i] = err;
                if (++info->received_size == info->total_size) {
                    try {
                        info->promise.set_value(true);
                    } catch (const std::exception& ex) {
                        info->promise.set_exception(std::make_exception_ptr(ex));
                    }
                }
            });
        }

        if (_session._timeout > 0) {
            auto ret = info->promise.get_future().wait_for(
                    std::chrono::milliseconds(_session._timeout));
            if (ret == std::future_status::timeout) {
                _session._error_code = ESessionError::READ_FAIL;
                break;
            }
        } else {
            info->promise.get_future().wait();
        }

        std::stringstream ss;
        for (size_t i = 0; i < info->status.size(); ++i) {
            if (info->status[i] != ESessionError::SESSION_OK) {
                ss << "request[" << i << "] error["
                   << s_session_error_info[static_cast<int>(info->status[i])] << "] ";
                _session._error_code = ESessionError::READ_FAIL;
            }
            _additional_err_text = std::move(ss.str());
        }
    } while (0);

    return *this;
}

}

