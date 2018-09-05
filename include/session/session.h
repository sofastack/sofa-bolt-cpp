// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/4.
//

#ifndef RPC_INCLUDE_SESSION_H
#define RPC_INCLUDE_SESSION_H

#include <functional>
#include "channel/channel.h"
#include "protocol/request_base.h"
#include "protocol/response_base.h"

namespace antflash {

using SessionAsyncCallback = std::function<void(ESessionError, ResponseBase*)>;
struct SocketReadSession;

class Session final {
friend class SocketManager;
friend class PipelineSession;
public:
    Session() : _timeout(-1),
                _retry(-1),
                _protocol(nullptr),
                _request(nullptr),
                _response(nullptr),
                _error_code(ESessionError::SESSION_OK),
                _channel(nullptr) {}
    ~Session();

    //Set request data to be sent. Before session sync/async function returns,
    //DO NOT release request's memory as session just hold reference of this
    //request.
    Session& send(const RequestBase& request) {
        _request = &request;
        return *this;
    }

    //Set channel which request data will be sent to, it's safe to set one channel
    //to several sessions in multiple threads.Before session sync/async function
    //returns, DO NOT release channel's memory.
    Session& to(Channel& channel);

    //Set timeout to this session, and this timeout is higher priority than channel's
    //timeout, if not set, use channel's timeout by default.
    inline Session& timeout(int32_t timeout) {
        _timeout = timeout;
        return *this;
    }

    //NOT USE RIGHT NOW
    inline Session& maxRetry(int32_t retry) {
        _retry = retry;
        return *this;
    }

    //Set response to store data that session receive from server. Before session
    // sync/async function returns, DO NOT release request's memory as session
    // just hold reference of this response.
    inline Session& receiveTo(ResponseBase& response) {
        _response = &response;
        return *this;
    }

    //Send data to server synchronized, block the thread calling this sync function.
    Session& sync();

    Session& async(SessionAsyncCallback callback);

    //Check if session sync/async function success, if failed, use getErrText to
    //get detail information.
    bool failed() {
        return _error_code != ESessionError::SESSION_OK;
    }

    //return detail information of session.
    const std::string& getErrText() const;

    //If a session is used to send data sync/async, remember to reset this session
    //before next calling.
    void reset();

    static const std::string& getErrText(ESessionError);

private:
    //Only be used in SocketManager
    Session& to(std::shared_ptr<Socket>& socket);

    void sendInternalWithRetry(SessionAsyncCallback* callback);
    void sendInternal(SessionAsyncCallback* callback);

    int32_t _timeout;
    int32_t _retry;
    size_t _begin_time_us;

    const Protocol* _protocol;
    const RequestBase* _request;
    ResponseBase* _response;

    ESessionError _error_code;

    Channel* _channel;
    std::shared_ptr<Socket> _socket;
};

class PipelineSession {
public:
    PipelineSession() : _channel(nullptr) {}
    ~PipelineSession() {}

    //Set channel which request data will be sent to, it's safe to set one channel
    //to several sessions in multiple threads.Before pipe session sync/async
    //returns, DO NOT release channel's memory.
    inline PipelineSession& to(Channel& channel) {
        _channel = &channel;
        return *this;
    }

    //Set timeout to this session, and this timeout is higher priority than channel's
    //timeout, if not set, use channel's timeout by default.
    inline PipelineSession& timeout(int32_t timeout) {
        _session.timeout(timeout);
        return *this;
    }

    inline PipelineSession& reserve(size_t size) {
        _requests.reserve(size);
        _responses.reserve(size);
        return *this;
    }

    inline PipelineSession& pipe(const RequestBase &req, ResponseBase &rsp) {
        _requests.emplace_back(&req);
        _responses.emplace_back(&rsp);
        return *this;
    }

    PipelineSession& sync();

    //Check if session sync/async function success, if failed, use getErrText to
    //get detail information.
    inline bool failed() {
        return _session.failed();
    }

    //return detail information of session.
    inline std::string getErrText() const {
        return _session.getErrText() + _additional_err_text;
    }

private:
    std::vector<const RequestBase*> _requests;
    std::vector<ResponseBase*> _responses;
    Session _session;
    Channel* _channel;
    std::string _additional_err_text;
};

}

#endif  //RPC_INCLUDE_SESSION_H
