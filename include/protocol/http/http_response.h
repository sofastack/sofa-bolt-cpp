// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/17.
//

#ifndef RPC_INCLUDE_HTTP_RESPONSE_H
#define RPC_INCLUDE_HTTP_RESPONSE_H

#include <unordered_map>
#include "http_header.h"
#include "protocol/response_base.h"

struct http_parser;

namespace antflash {

enum EHttpStatus {
    HTTP_STATUS_CONTINUE = 100,
    HTTP_STATUS_OK = 200,
    HTTP_STATUS_CREATED = 201,
    HTTP_STATUS_ACCEPTED = 202,
    HTTP_STATUS_BAD_REQUEST = 400,
    HTTP_STATUS_FORBIDDEN = 403,
    HTTP_STATUS_NOT_FOUND = 404,
};

class HttpResponse : public ResponseBase {
public:
    HttpResponse() : _status(HTTP_STATUS_OK), _parser(nullptr) {}
    ~HttpResponse();

    const std::string& body() const {
        return _body;
    }

    EResParseResult deserialize(IOBuffer &buffer) noexcept override;
    static EResParseResult checkHeader(
            IOBuffer &buffer,
            size_t& data_size,
            size_t& request_id,
            void** parser);

    void setHttpParser(void*);

    static int onMessageBegin(struct http_parser*);
    static int onUrl(struct http_parser *, const char *, size_t);
    static int onStatus(struct http_parser*, const char *, size_t);
    static int onHeaderField(struct http_parser *, const char *, size_t);
    static int onHeaderValue(struct http_parser *, const char *, size_t);
    static int onHeadersComplete(struct http_parser *);
    static int onBodyCb(struct http_parser*, const char *, size_t);
    static int onMessageCompleteCb(struct http_parser *);

private:
    EHttpStatus _status;
    struct http_parser* _parser;

    std::string _url;
    std::string _body;
    std::unordered_map<std::string, std::string> _header;
};

}


#endif //RPC_INCLUDE_HTTP_RESPONSE_H
