// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/17.
//

#include "protocol/http/http_header.h"

namespace antflash {

const std::string HttpHeader::_s_method_name[HTTP_METHOD_TOTAL] = {
        "GET",
        "POST",
};

const std::string HttpHeader::_s_header_key[HTTP_HEADER_KEY_SIZE] = {
        "log-id",
        "application/json",
        "connection",
};

}
