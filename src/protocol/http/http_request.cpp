// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/17.
//

#include "protocol/http/http_request.h"
#include <unordered_map>
#include <sstream>
#include "common/io_buffer.h"
#include "common/log.h"

namespace antflash {

HttpRequest& HttpRequest::attach(const std::string& data) {
    _data.reset(new IOBuffer);
    _data->append(data);
    return *this;
}

HttpRequest& HttpRequest::attach(const char* data) {
    _data.reset(new IOBuffer);
    _data->append(data);
    return *this;
}

std::shared_ptr<IOBuffer> HttpRequest::attach() const {
    return _data;
}

size_t HttpRequest::attach_size() const {
    if (_data) {
        return _data->size();
    }

    return 0;
}

}
