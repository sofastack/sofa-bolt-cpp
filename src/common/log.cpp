// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/7/17.
//

#include "log.h"
#include <string>
#include "rpc.h"

namespace antflash {

static const char* s_level_names[] = {"DEBUG", "INFO", "WARNING", "ERROR", "FATAL"};

inline void DefaultLogHandler(
        LogLevel level, const char* filename, int line,
        const std::string& message) {

    fprintf(stdout, "[bolt-rpc %s %s:%d] %s\n",
            s_level_names[static_cast<size_t>(level)],
            filename, line, message.c_str());
    fflush(stdout);
}

static LogHandler* s_cur_log_handler = &DefaultLogHandler;

LogHandler* setLogHandler(LogHandler* handler) {
    LogHandler* old = s_cur_log_handler;
    s_cur_log_handler = handler;
    return old;
}

void setLogLevel(LogLevel level) {
    LogMessage::setLogLevel(level);
}

LogLevel LogMessage::_s_min_level = LogLevel::LOG_LEVEL_INFO;

void LogMessage::finish() {
    if (_level < _s_min_level) {
        return;
    }
    if (nullptr != s_cur_log_handler) {
        s_cur_log_handler(_level, _filename, _line, _message);
    }
}

}
