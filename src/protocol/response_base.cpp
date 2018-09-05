// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/11.
//

#include "protocol/response_base.h"

namespace antflash {

const char* ParseResultToString(EResParseResult r) {
    static const char* msg[] = {
            "ok",
            "parse error",
            "not enough data"
    };
    return msg[static_cast<int>(r)];
}

}
