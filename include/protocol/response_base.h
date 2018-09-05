// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/11.
//

#ifndef RPC_INCLUDE_RESPONSE_BASE_H
#define RPC_INCLUDE_RESPONSE_BASE_H

namespace antflash {

class IOBuffer;

enum class EResParseResult {
    PARSE_OK = 0,
    PARSE_ERROR,
    PARSE_NOT_ENOUGH_DATA,
};

class ResponseBase {
public:
    ResponseBase() {}
    virtual ~ResponseBase() {}

    virtual EResParseResult deserialize(IOBuffer &buffer) noexcept = 0;
};

const char* ParseResultToString(EResParseResult r);

}
#endif //RPC_INCLUDE_RESPONSE_BASE_H
