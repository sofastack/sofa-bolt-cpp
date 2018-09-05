// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/25.
//

#include "schedule/schedule.h"
#include <channel/channel.h>
#include "tcp/socket.h"
#include "tcp/socket_manager.h"
#include "simple_socket_server.h"
#include <protocol/bolt/bolt_response.h>
#include "protocol/bolt/bolt_protocol.h"
#include <gtest/gtest.h>
#include "schedule/loop_thread.h"

namespace antflash {
class ChannelUnitTest {
public:
    ChannelUnitTest(Channel& c) {
        c.getSocket(_socket);
    }

    bool active() const {
        if (_socket) {
            return _socket->active();
        }
        return false;
    }

    bool tryExclusive() {
        if (_socket) {
            bool ret = _socket->tryExclusive();
            if (ret) {
                _socket->releaseExclusive();
            }
            return ret;
        }
        return false;
    }

private:
    std::shared_ptr<Socket> _socket;
};
}
using namespace antflash;

struct [[gnu::packed]] BoltHeaderTest {
    uint8_t proto;
    uint8_t type;
    uint16_t cmdcode;
    uint8_t ver2;
    uint32_t request_id;
    uint8_t codec;

    uint16_t status;
    uint16_t class_len;
    uint16_t header_len;
    uint32_t content_len;

    void ntoh() {
        cmdcode = ntohs(cmdcode);
        request_id = ntohl(request_id);
        status = ntohs(status);
        class_len = ntohs(class_len);
        header_len = ntohs(header_len);
        content_len = ntohl(content_len);
    }
};

TEST(SocketTest, base) {
    EndPoint end_point;
    end_point.parseFromString("1.2.3.4:1234");
    ASSERT_STREQ(end_point.ipToStr().c_str(), "1.2.3.4:1234");
    auto so = std::make_shared<Socket>(end_point);
    bool ret = so->connect(100);
    ASSERT_FALSE(ret);
    IOBuffer buffer;
    ASSERT_FALSE(so->write(buffer, -1));
    ASSERT_FALSE(so->write(buffer, 1));
    buffer.append("abc");
    //ASSERT_FALSE(so->write(buffer, 1));

    LoopThread thread;
    LoopThread thread1(std::move(thread));
    LoopThread thread2;
    thread2 = std::move(thread1);
}

TEST(BoltResponseTest, base) {
    BoltResponse response;
    IOBuffer empty;
    size_t data_size = 0;
    size_t request_id = 0;
    ASSERT_EQ(response.checkHeader(empty, data_size, request_id),
              EResParseResult::PARSE_NOT_ENOUGH_DATA);
    ASSERT_EQ(response.deserialize(empty),
              EResParseResult::PARSE_NOT_ENOUGH_DATA);
    BoltHeaderTest header;
    header.proto = BOLT_PROTOCOL_TYPE + 1;
    header.content_len = 64 * 1024 * 1024;
    header.class_len = 10;
    header.header_len = 0;
    header.ntoh();
    empty.append((const void*)&header, sizeof(header));
    ASSERT_EQ(response.checkHeader(empty, data_size, request_id),
              EResParseResult::PARSE_ERROR);
    ASSERT_EQ(response.deserialize(empty),
              EResParseResult::PARSE_ERROR);

    header.proto = BOLT_PROTOCOL_TYPE;
    header.type = BOLT_PROTOCOL_REQUEST;
    header.content_len = 0;
    header.class_len = 0;
    header.header_len = 0;
    header.ntoh();
    empty.append((const void*)&header, sizeof(header));
    ASSERT_EQ(response.deserialize(empty),
              EResParseResult::PARSE_ERROR);

    header.proto = BOLT_PROTOCOL_TYPE;
    header.type = BOLT_PROTOCOL_RESPONSE;
    header.cmdcode = BOLT_PROTOCOL_CMD_HEARTBEAT;
    header.content_len = 0;
    header.class_len = 0;
    header.header_len = 0;
    header.status = BoltResponse::SUCCESS;
    header.ntoh();
    empty.append((const void*)&header, sizeof(header));
    ASSERT_EQ(response.deserialize(empty),
              EResParseResult::PARSE_OK);
    ASSERT_EQ(response.msg(), "success");

    std::shared_ptr<antflash::ResponseBase> response2;
    response2 = std::make_shared<antflash::BoltResponse>();
    header.proto = BOLT_PROTOCOL_TYPE;
    header.type = BOLT_PROTOCOL_RESPONSE;
    header.cmdcode = BOLT_PROTOCOL_CMD_HEARTBEAT;
    header.content_len = 0;
    header.class_len = 0;
    header.header_len = 0;
    header.status = BoltResponse::SUCCESS;
    header.ntoh();
    empty.append((const void*)&header, sizeof(header));
    response2->deserialize(empty);
    ASSERT_TRUE(antflash::bolt::parseHeartbeatResponse(response2));

    header.proto = BOLT_PROTOCOL_TYPE;
    header.type = BOLT_PROTOCOL_RESPONSE;
    header.cmdcode = BOLT_PROTOCOL_CMD_HEARTBEAT;
    header.content_len = 0;
    header.class_len = 0;
    header.header_len = 0;
    header.status = 0x000A;
    header.ntoh();
    empty.append((const void*)&header, sizeof(header));
    ASSERT_EQ(response.deserialize(empty),
              EResParseResult::PARSE_OK);
    ASSERT_EQ(response.msg(), "unknown");

    header.proto = BOLT_PROTOCOL_TYPE;
    header.type = BOLT_PROTOCOL_RESPONSE;
    header.cmdcode = BOLT_PROTOCOL_CMD_REQUEST;
    header.content_len = 0;
    header.class_len = 0;
    header.header_len = 0;
    header.ntoh();
    empty.append((const void*)&header, sizeof(header));
    ASSERT_EQ(response.deserialize(empty),
              EResParseResult::PARSE_ERROR);

    header.proto = BOLT_PROTOCOL_TYPE;
    header.type = BOLT_PROTOCOL_RESPONSE;
    header.cmdcode = BOLT_PROTOCOL_CMD_RESPONSE;
    header.content_len = 0;
    header.class_len = 10;
    header.header_len = 0;
    header.status = BoltResponse::SUCCESS;
    header.ntoh();
    empty.append((const void*)&header, sizeof(header));
    ASSERT_EQ(response.deserialize(empty),
              EResParseResult::PARSE_NOT_ENOUGH_DATA);

    header.proto = BOLT_PROTOCOL_TYPE;
    header.type = BOLT_PROTOCOL_RESPONSE;
    header.cmdcode = BOLT_PROTOCOL_CMD_RESPONSE;
    header.content_len = 0;
    header.class_len = 0;
    header.header_len = 10;
    header.status = BoltResponse::SUCCESS;
    auto header2 = header;
    header.ntoh();
    empty.append((const void*)&header, sizeof(header));
    ASSERT_EQ(response.deserialize(empty),
              EResParseResult::PARSE_NOT_ENOUGH_DATA);
    header2.status = BoltResponse::SERVER_SERIAL_EXCEPTION;
    header2.ntoh();
    empty.append((const void*)&header2, sizeof(header2));
    ASSERT_EQ(response.deserialize(empty),
              EResParseResult::PARSE_OK);


    header.proto = BOLT_PROTOCOL_TYPE;
    header.type = BOLT_PROTOCOL_RESPONSE;
    header.cmdcode = BOLT_PROTOCOL_CMD_RESPONSE;
    header.content_len = 0;
    header.class_len = 0;
    header.status = BoltResponse::SUCCESS;
    header.header_len = sizeof(uint32_t);
    header.ntoh();
    empty.append((const void*)&header, sizeof(header));
    uint32_t header_key_size = 0;
    header_key_size = htonl(header_key_size);
    empty.append((const void*)&header_key_size, sizeof(uint32_t));
    ASSERT_EQ(response.deserialize(empty),
              EResParseResult::PARSE_NOT_ENOUGH_DATA);

    header.proto = BOLT_PROTOCOL_TYPE;
    header.type = BOLT_PROTOCOL_RESPONSE;
    header.cmdcode = BOLT_PROTOCOL_CMD_RESPONSE;
    header.content_len = 0;
    header.class_len = 0;
    header.status = BoltResponse::SUCCESS;
    header.header_len = sizeof(uint32_t);
    header.ntoh();
    empty.append((const void*)&header, sizeof(header));
    header_key_size = 10;
    header_key_size = htonl(header_key_size);
    empty.append((const void*)&header_key_size, sizeof(uint32_t));
    ASSERT_EQ(response.deserialize(empty),
              EResParseResult::PARSE_NOT_ENOUGH_DATA);

    header.proto = BOLT_PROTOCOL_TYPE;
    header.type = BOLT_PROTOCOL_RESPONSE;
    header.cmdcode = BOLT_PROTOCOL_CMD_RESPONSE;
    header.content_len = 0;
    header.class_len = 0;
    header.status = BoltResponse::SUCCESS;
    header.header_len = sizeof(uint32_t) * 2;
    header.ntoh();
    empty.append((const void*)&header, sizeof(header));
    header_key_size = 0;
    header_key_size = htonl(header_key_size);
    empty.append((const void*)&header_key_size, sizeof(uint32_t));
    empty.append((const void*)&header_key_size, sizeof(uint32_t));
    ASSERT_EQ(response.deserialize(empty),
              EResParseResult::PARSE_OK);

    header.proto = BOLT_PROTOCOL_TYPE;
    header.type = BOLT_PROTOCOL_RESPONSE;
    header.cmdcode = BOLT_PROTOCOL_CMD_RESPONSE;
    header.status = BoltResponse::SUCCESS;
    header.content_len = 0;
    header.class_len = 0;
    header_key_size = 3;
    header.header_len = (sizeof(uint32_t) + header_key_size) * 2 ;
    header.ntoh();
    header_key_size = htonl(header_key_size);
    empty.append((const void*)&header, sizeof(header));
    empty.append((const void*)&header_key_size, sizeof(uint32_t));
    empty.append("key");
    empty.append((const void*)&header_key_size, sizeof(uint32_t));
    empty.append("666");
    ASSERT_EQ(response.deserialize(empty),
              EResParseResult::PARSE_OK);

    header.proto = BOLT_PROTOCOL_TYPE;
    header.type = BOLT_PROTOCOL_RESPONSE;
    header.cmdcode = BOLT_PROTOCOL_CMD_RESPONSE;
    header.status = BoltResponse::SUCCESS;
    header.content_len = 0;
    header.class_len = 0;
    header_key_size = 3;
    header.header_len = (sizeof(uint32_t) + header_key_size) * 2 ;
    header.ntoh();
    header_key_size = htonl(header_key_size);
    empty.append((const void*)&header, sizeof(header));
    empty.append((const void*)&header_key_size, sizeof(uint32_t));
    empty.append("key");
    empty.append((const void*)&header_key_size, sizeof(uint32_t));
    empty.append("66");
    ASSERT_EQ(response.deserialize(empty),
              EResParseResult::PARSE_NOT_ENOUGH_DATA);

    std::string ss;
    BoltResponse str_rsp(ss);
    header.proto = BOLT_PROTOCOL_TYPE;
    header.type = BOLT_PROTOCOL_RESPONSE;
    header.cmdcode = BOLT_PROTOCOL_CMD_RESPONSE;
    header.status = BoltResponse::SUCCESS;
    header.content_len = 3;
    header.class_len = 0;
    header.header_len = 0;
    header.ntoh();
    empty.append((const void*)&header, sizeof(header));
    empty.append("666");

    ASSERT_EQ(str_rsp.deserialize(empty),
              EResParseResult::PARSE_OK);
    ASSERT_STREQ(ss.c_str(), "666");

    std::string ss2;
    BoltResponse str_rsp2(ss2);
    header.proto = BOLT_PROTOCOL_TYPE;
    header.type = BOLT_PROTOCOL_RESPONSE;
    header.cmdcode = BOLT_PROTOCOL_CMD_RESPONSE;
    header.status = BoltResponse::SUCCESS;
    header.content_len = 3;
    header.class_len = 0;
    header.header_len = 0;
    header.ntoh();
    empty.append((const void*)&header, sizeof(header));
    empty.append("66");

    ASSERT_EQ(str_rsp2.deserialize(empty),
              EResParseResult::PARSE_NOT_ENOUGH_DATA);
}

TEST(SocketManagerTest, base) {

    SimpleSocketServer server;
    std::thread ts([&server](){
        EXPECT_TRUE(server.run(6329));
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(Schedule::getInstance().init());
    EXPECT_TRUE(SocketManager::getInstance().init());

    {
        ChannelOptions options;
        ChannelOptions options_copy(options);
        Channel channel;
        EXPECT_FALSE(channel.init("illegal address here", &options_copy));
    }

    {
        Channel channel;
        EXPECT_TRUE(channel.init("127.0.0.1:6329", nullptr));

        ChannelUnitTest test(channel);
        EXPECT_TRUE(test.active());
        EXPECT_FALSE(test.tryExclusive());

        std::cout << "sleep for a while" << SOCKET_MAX_IDLE_US << "us" << std::endl;
        server.stop();
        std::this_thread::sleep_for(std::chrono::microseconds(SOCKET_MAX_IDLE_US));
        std::this_thread::sleep_for(std::chrono::seconds(2));
        EXPECT_FALSE(test.active());
        EXPECT_FALSE(test.tryExclusive());
        ChannelUnitTest test2(channel);
        EXPECT_FALSE(test2.active());
        EXPECT_FALSE(test2.tryExclusive());
        EXPECT_TRUE(test.tryExclusive());
    }

    ts.join();

    {
        Channel channel;
        EXPECT_FALSE(channel.init("127.0.0.1:6329", nullptr));
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    Schedule::getInstance().destroy_schedule();
    SocketManager::getInstance().destroy();
    Schedule::getInstance().destroy_time_schedule();
}
