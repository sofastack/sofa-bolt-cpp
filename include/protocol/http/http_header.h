// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/17.
//

#ifndef RPC_INCLUDE_HTTP_HEADER_H
#define RPC_INCLUDE_HTTP_HEADER_H

#include <string>

namespace antflash {

enum EHttpMethod {
    HTTP_METHOD_GET = 0,
    HTTP_METHOD_POST,
    HTTP_METHOD_TOTAL
};

enum EHttpHeaderKey {
    LOG_ID = 0,
    CONTENT_TYPE_JSON,
    CONNECTION,
    HTTP_HEADER_KEY_SIZE
};

class HttpHeader {
public:
    inline static const std::string& getHeaderKey(EHttpHeaderKey key) {
        return _s_header_key[key];
    }

    inline static const std::string& getMethod(EHttpMethod key) {
        return _s_method_name[key];
    }

private:
    static const std::string _s_header_key[HTTP_HEADER_KEY_SIZE];
    static const std::string _s_method_name[HTTP_METHOD_TOTAL];
};

}

#endif //RPC_HTTP_HEADER_H
