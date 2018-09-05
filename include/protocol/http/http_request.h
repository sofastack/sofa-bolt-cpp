// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/17.
//

#ifndef RPC_INCLUDE_HTTP_REQUEST_H
#define RPC_INCLUDE_HTTP_REQUEST_H

#include <memory>
#include <string>
#include "protocol/request_base.h"
#include "common/uri.h"
#include "http_header.h"

namespace antflash {

class IOBuffer;
namespace http {
class HttpInternalRequest;
}

class HttpRequest : public RequestBase {
friend class http::HttpInternalRequest;
public:
    HttpRequest() {
    }

    const URI& uri() const {
        return _uri;
    }

    HttpRequest& uri(const std::string& uri) {
        _uri = uri;
        return *this;
    }
    HttpRequest& method(EHttpMethod method) {
        _method = method;
        return *this;
    }
    HttpRequest& attach(const std::string& data);
    HttpRequest& attach(const char* data);
    std::shared_ptr<IOBuffer> attach() const;
    size_t attach_size() const;

    HttpRequest& attach_type(const std::string& type) {
        _attch_type = type;
        return *this;
    }
    HttpRequest& attach_type(EHttpHeaderKey type) {
        _attch_type = HttpHeader::getHeaderKey(type);
        return *this;
    }

    const std::string& attach_type() const {
        return _attch_type;
    }

    //current 1.1
    //HttpRequest& version(int32_t major, int32_t minor) {
    //    return *this;
    //}

private:
    enum class EDataType {
        CSTRING,
        STRING,
    };

    URI _uri;
    EHttpMethod _method;
    std::string _attch_type;
    std::shared_ptr<IOBuffer> _data;
};

}

#endif //RPC_INCLUDE_HTTP_REQUEST_H
