// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/4.
//

#ifndef RPC_INCLUDE_REQUEST_BASE_H
#define RPC_INCLUDE_REQUEST_BASE_H

namespace antflash {

class IOBuffer;

//abstract
class RequestBase {
public:
    RequestBase() {}
    virtual ~RequestBase() {}
};

}

#endif //RPC_INCLUDE_REQUEST_BASE_H
