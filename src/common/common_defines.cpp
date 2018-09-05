// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/18.
//

#include "common/common_defines.h"
#include <signal.h>
#include "rpc.h"
#include "schedule/schedule.h"
#include "tcp/socket_manager.h"
#include "log.h"
#include <stdio.h>

namespace antflash {

struct RpcErrorInfo {
    const char *name;
    const char *description;
};

#define XX(num, name, string) { "RPC_STATUS_" #name, #string },
static RpcErrorInfo s_rpc_status_str_tab[] = {
        RPC_STATUS_MAP(XX)
};
#undef XX

/*
static void ProtoBufLogHandler(google::protobuf::LogLevel level,
                                     const char* filename, int line,
                                     const std::string& message) {
    switch (level) {
    case google::protobuf::LOGLEVEL_INFO:
        LOG_INFO("{}:{} {}", filename, line, message);
        return;
    case google::protobuf::LOGLEVEL_WARNING:
        LOG_WARN("{}:{} {}", filename, line, message);
        return;
    case google::protobuf::LOGLEVEL_ERROR:
        LOG_ERROR("{}:{} {}", filename, line, message);
        return;
    case google::protobuf::LOGLEVEL_FATAL:
        LOG_FATAL("{}:{} {}", filename, line, message);
        return;
    default:
        LOG_DEBUG("{}:{} {}", filename, line, message);
        return;
    }   
}
*/

bool globalInit() {
    //google::protobuf::SetLogHandler(&ProtoBufLogHandler);

    //Init schedule first so that sockets can be connected/read normally
    if (!Schedule::getInstance().init()) {
        return false;
    }

    if (!SocketManager::getInstance().init()) {
        return false;
    }

#if defined(OS_MACOSX)
#else
    // Ignore SIGPIPE.
    struct sigaction oldact;
    if (sigaction(SIGPIPE, nullptr, &oldact) != 0 ||
        (oldact.sa_handler == nullptr && oldact.sa_sigaction == nullptr)) {
        LOG_DEBUG("ignore SIGPIPE");
        signal(SIGPIPE, SIG_IGN);
    }
#endif

    return true;
}

void globalDestroy() {
    //Destroy schedule first so that sockets can be reclaimed normally
    Schedule::getInstance().destroy_schedule();

    SocketManager::getInstance().destroy();

    //timeout thread must by destroyed after socket manager
    Schedule::getInstance().destroy_time_schedule();
}

const char* getRpcStatus(ERpcStatus status) {
    return s_rpc_status_str_tab[status].description;
}

}

