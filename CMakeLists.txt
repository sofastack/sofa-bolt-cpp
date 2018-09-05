cmake_minimum_required(VERSION 3.10)
project(bolt-rpc-client)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
if(PROTOBUF_FOUND)
    message(STATUS "protobuf library found")
else()
    message(FATAL_ERROR "protobuf library is needed but cant be found")
endif()

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++1y -fprofile-arcs -ftest-coverage")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++1y")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__FILENAME__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")

include_directories(${PROTOBUF_INCLUDE_DIRS})
include_directories(second_party/fmt/include)
include_directories(
        include
        src)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})
PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS
        example/sample.proto)

set(FMT_SOURCE
        second_party/fmt/src/format.cc)

set(RPC_SOURCE
        src/tcp/socket.cpp
        src/tcp/endpoint.cpp
        src/tcp/socket_manager.cpp
        src/tcp/socket_posix.cpp
        src/session/session.cpp
        src/protocol/protocol_define.cpp
        src/protocol/bolt/bolt_protocol.cpp
        src/protocol/bolt/bolt_response.cpp
        src/channel/channel.cpp
        src/schedule/loop_thread.cpp
        src/schedule/schedule.cpp
        src/schedule/time_thread.cpp
        src/common/log.cpp
        src/common/common_defines.cpp
        src/common/io_buffer.cpp
        src/common/uri.cpp
        src/protocol/http/http_parser.c
        src/protocol/http/http_header.cpp
        src/protocol/http/http_request.cpp
        src/protocol/http/http_response.cpp
        src/protocol/http/http_protocol.cpp )

SET(POLL_SOURCE
		src/schedule/epoll_loop.cpp)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    add_definitions(-DOS_LINUX)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_definitions(-DOS_MACOSX)
    SET(POLL_SOURCE
            src/schedule/kqueue_loop.cpp)
endif()

add_library(bolt-rpc-client STATIC ${FMT_SOURCE} ${RPC_SOURCE} ${POLL_SOURCE})
target_link_libraries(bolt-rpc-client Threads::Threads z ${PROTOBUF_LIBRARIES})

add_executable(single_request_example ${PROTO_SRCS}
	        example/single_request_example.cpp)
#target_link_libraries(single_request_example bolt-rpc-client Threads::Threads alog z tcmalloc profiler ${PROTOBUF_LIBRARIES})
target_link_libraries(single_request_example bolt-rpc-client Threads::Threads ${PROTOBUF_LIBRARIES})

add_executable(bolt_heartbeat ${PROTO_SRCS}
	        example/heartbeat_example.cpp)
target_link_libraries(bolt_heartbeat bolt-rpc-client Threads::Threads ${PROTOBUF_LIBRARIES})

add_executable(cloud_rpc_demo ${PROTO_SRCS}
        example/cloud_rpc_demo.cpp)
target_link_libraries(cloud_rpc_demo bolt-rpc-client Threads::Threads ${PROTOBUF_LIBRARIES})

set(UNIT_TEST_SOURCE
        test/unit_test/unittest_main.cpp
        test/unit_test/endpoint_unittest.cpp
        test/unit_test/channel_unittest.cpp
        test/unit_test/io_buffer_unittest.cpp
        test/unit_test/lifecyclelock_unittest.cpp
        test/unit_test/mpscqueue_unittest.cpp
        test/unit_test/simple_socket_server.cpp
        test/unit_test/socket_manager_unittest.cpp
        test/unit_test/socket_posix_unittest.cpp
        test/unit_test/thread_pool_unittest.cpp
        test/unit_test/time_thread_unittest.cpp
        test/unit_test/uri_unittest.cpp
        test/unit_test/utils_unittest.cpp
        test/unit_test/http_unittest.cpp
        test/unit_test/intrusivelist_unittest.cpp
        test/unit_test/concurrenthashmap_unittest.cpp
        test/unit_test/lrucache_unittest.cpp)

PROTOBUF_GENERATE_CPP(TEST_PROTO_SRCS TEST_PROTO_HDRS
        test/unit_test/io_buffer_unittest.proto)

include_directories(
    src)

add_executable(unit_test ${UNIT_TEST_SOURCE} ${TEST_PROTO_HDRS} ${TEST_PROTO_SRCS})
target_link_libraries(unit_test gtest bolt-rpc-client ${PROTOBUF_LIBRARIES})
