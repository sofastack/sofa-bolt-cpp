// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/10.
//

#ifndef RPC_COMMON_LOG_H
#define RPC_COMMON_LOG_H

#include <fmt/format.h>
#include "common/common_defines.h"

namespace antflash {

class LogMessage {
public:
    LogMessage(LogLevel level, const char* filename, int line):
            _level(level), _filename(filename), _line(line) {}
    ~LogMessage() {
        finish();
    }

    template<typename... Args>
    void print(Args&& ...args) {
        _message = fmt::format(std::forward<Args>(args)...);
    };

    static void setLogLevel(LogLevel level) {
        _s_min_level = level;
    }

private:
    void finish();

    LogLevel _level;
    const char* _filename;
    int _line;
    std::string _message;

    static LogLevel _s_min_level;
};

}

#ifndef __FILENAME__
#define __FILENAME__ __FILE__
#endif

#define LOG(LEVEL, FORMAT, ARGS...) \
antflash::LogMessage(antflash::LogLevel::LOG_LEVEL_##LEVEL, __FILENAME__, __LINE__).print(FORMAT, ##ARGS)

#define LOG_IF(LEVEL, CONDITION, FORMAT, ARGS...) \
!(CONDITION) ? (void)0 : LOG(LEVEL, FORMAT, ##ARGS)

#define LOG_DEBUG(FORMAT, ARGS...) \
LOG(DEBUG, FORMAT, ##ARGS)

#define LOG_INFO(FORMAT, ARGS...) \
LOG(INFO, FORMAT, ##ARGS)

#define LOG_WARN(FORMAT, ARGS...) \
LOG(WARNING, FORMAT, ##ARGS)

#define LOG_ERROR(FORMAT, ARGS...) \
LOG(ERROR, FORMAT, ##ARGS)

#define LOG_FATAL(FORMAT, ARGS...) \
LOG(FATAL, FORMAT, ##ARGS)

#endif //RPC_COMMON_LOG_H
