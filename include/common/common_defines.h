// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/2.
//

#ifndef RPC_INCLUDE_COMMON_DEFINES_H
#define RPC_INCLUDE_COMMON_DEFINES_H

#include <string>

namespace antflash {

/**
 * Log Level enum
 */
enum class LogLevel {
    LOG_LEVEL_DEBUG,   // Debug.
    LOG_LEVEL_INFO,    // Informational.
    LOG_LEVEL_WARNING, // Warns about issues that, although not technically a
                       // problem now, could cause problems in the future.
    LOG_LEVEL_ERROR,   //An error occurred which should never happen during normal use.
    LOG_LEVEL_FATAL    // An error occurred from which the library cannot recover.
};

/**
 * Log Handler function pointer
 */
using LogHandler = void(LogLevel level,
                           const char* filename, int line,
                           const std::string& message);

/*Socket Related*/
static constexpr auto CACHELINE_SIZE = 64;
static constexpr size_t BUFFER_DEFAULT_SLICE_SIZE = 8192;
static constexpr size_t BUFFER_DEFAULT_BLOCK_SIZE = 8192;
static constexpr size_t BUFFER_MAX_SLICE_SIZE = 65536;
static constexpr size_t MAX_POLL_EVENT = 32;
static constexpr int32_t SOCKET_CONNECT_TIMEOUT_MS = 200;
static constexpr int32_t SOCKET_TIMEOUT_MS = 500;
static constexpr int32_t SOCKET_MAX_RETRY = 3;
static constexpr size_t SOCKET_MAX_IDLE_US = 15 * 1000 * 1000;

static constexpr size_t MAX_PARALLEL_SESSION_SIZE_ON_SOCKET = 1024;

static constexpr size_t CONNECTION_POOL_MAX_SOCKET_SIZE = 32;

/*Bolt Protocol Related*/
static constexpr uint8_t BOLT_PROTOCOL_TYPE = 1;
static constexpr uint8_t BOLT_PROTOCOL_REQUEST = 1;
static constexpr uint8_t BOLT_PROTOCOL_RESPONSE = 0;
static constexpr uint8_t BOLT_PROTOCOL_RESPONSE_ONE_WAY = 2;
static constexpr uint16_t BOLT_PROTOCOL_CMD_HEARTBEAT = 0;
static constexpr uint16_t BOLT_PROTOCOL_CMD_REQUEST = 1;
static constexpr uint16_t BOLT_PROTOCOL_CMD_RESPONSE = 2;
static constexpr uint8_t BOLT_PROTOCOL_VER2 = 1;
static constexpr uint8_t BOLT_PROTOCOL_CODEC_PB = 11;

static constexpr auto SIZE_OF_AVOID_FALSE_SHARING = 64;

/* Zookeeper Related */
static constexpr size_t ZOOKEEPER_RECV_TIMEOUT = 30000;

/*Rpc Client All Status Codes Related*/
#define RPC_STATUS_MAP(XX)                                                      \
    XX(0, OK,                               Status ok)                          \
    XX(1, INIT,                             Status init)                        \
    XX(2, CHANNEL_INIT_FAIL,                Channel init fail)                  \
    XX(3, SOCKET_CONNECT_FAIL,              Connect server fail)                \
    XX(4, SOCKET_CONNECT_TIMEOUT,           Connect server timeout)             \
    XX(5, SOCKET_WRITE_ERROR,               Write to server error)              \
    XX(6, SOCKET_READ_ERROR,                Read from server error)             \
    XX(7, SOCKET_CLOSED_BY_SERVER,          Remote server close the connection) \
    XX(8, SOCKET_HEARTBEAT_FAIL,            Heartbeat fail)                     \
    XX(9, SOCKET_TO_RECLAIM,                Socket is prepared to be reclaimed) \

enum ERpcStatus : int32_t
{
#define XX(num, name, string) RPC_STATUS_##name = num,
    RPC_STATUS_MAP(XX)
#undef XX
};

const char* getRpcStatus(ERpcStatus status);

enum class ESessionError {
    SESSION_OK = 0,
    PROTOCOL_NOT_FOUND,
    ASSEMBLE_REQUEST_FAIL,
    SOCKET_LOST,
    SOCKET_BUSY,
    WRITE_FAIL,
    READ_FAIL,
    READ_TIMEOUT,
    PARSE_RESPONSE_FAIL,
    TIMER_BUSY
};

}

#endif //RPC_INCLUDE_COMMON_DEFINES_H
