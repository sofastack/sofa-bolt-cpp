// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/9.
//

#ifndef RPC_SCHEDULE_LOOP_H
#define RPC_SCHEDULE_LOOP_H

#include "tcp/socket_base.h"
#include <memory>

namespace antflash {

struct LoopInternalData;

class Loop {
public:
    Loop();
    ~Loop();

    bool init();
    void destroy();
    void loop_once();

    bool add_event(int fd, int events, void* handler);
    void remove_event(int fd, int events);

private:
    base::FdGuard _backend_fd;
    std::unique_ptr<LoopInternalData> _data;
};

}

#endif //RPC_SCHEDULE_LOOP_H
