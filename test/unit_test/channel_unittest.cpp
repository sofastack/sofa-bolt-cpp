// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/25.
//

#include <gtest/gtest.h>
#include <thread>
#include "rpc.h"
#include "tcp/socket.h"

using namespace antflash;

namespace antflash {
class ChannelUnitTest {
public:
    ChannelUnitTest(Channel& c) {
        _socket.reset();
        c.getSocket(_socket);
    }

    std::shared_ptr<Socket>& socket() {
        return _socket;
    }

private:
    std::shared_ptr<Socket> _socket;
};
}

TEST(ChannelTest, initSingle) {
    ASSERT_TRUE(globalInit());

    Channel channel;
    Channel& channel2 = channel;
    channel2 = channel;

    ASSERT_EQ(channel.status(), ERpcStatus::RPC_STATUS_INIT);
    ASSERT_STREQ(antflash::getRpcStatus(channel.status()), "Status init");

    channel.init("127.0.0.1:12345", nullptr);
    auto& option = channel.option();
    ASSERT_EQ(option.connection_type, EConnectionType::CONNECTION_TYPE_SINGLE);
    ASSERT_EQ(option.protocol, EProtocolType::PROTOCOL_BOLT);
    ASSERT_EQ(option.connect_timeout_ms, SOCKET_CONNECT_TIMEOUT_MS);
    ASSERT_EQ(option.timeout_ms, SOCKET_TIMEOUT_MS);
    ASSERT_EQ(option.max_retry, SOCKET_MAX_RETRY);

    ASSERT_NE(channel.status(), ERpcStatus::RPC_STATUS_INIT);

    ChannelUnitTest channel_test(channel);
    ASSERT_FALSE(channel_test.socket());

    globalDestroy();
}

TEST(ChannelTest, initShort) {
    ASSERT_TRUE(globalInit());

    ChannelOptions options;
    options.protocol = EProtocolType::PROTOCOL_HTTP;
    options.connection_type = EConnectionType::CONNECTION_TYPE_SHORT;

    Channel channel;
    ASSERT_EQ(channel.status(), ERpcStatus::RPC_STATUS_INIT);
    channel.init("127.0.0.1:12200", &options);

    auto& option = channel.option();
    ASSERT_EQ(option.connection_type, EConnectionType::CONNECTION_TYPE_SHORT);
    ASSERT_EQ(option.protocol, EProtocolType::PROTOCOL_HTTP);
    ASSERT_EQ(option.connect_timeout_ms, SOCKET_CONNECT_TIMEOUT_MS);
    ASSERT_EQ(option.timeout_ms, SOCKET_TIMEOUT_MS);
    ASSERT_EQ(option.max_retry, SOCKET_MAX_RETRY);

    ChannelUnitTest channel_test(channel);
    ASSERT_TRUE(!!channel_test.socket());

    globalDestroy();
}

TEST(ChannelTest, initPool) {
    ASSERT_TRUE(globalInit());

    ChannelOptions options;
    options.protocol = EProtocolType::PROTOCOL_HTTP;
    options.connection_type = EConnectionType::CONNECTION_TYPE_POOLED;

    Channel channel;
    ASSERT_EQ(channel.status(), ERpcStatus::RPC_STATUS_INIT);
    channel.init("127.0.0.1:12200", &options);

    auto& option = channel.option();
    ASSERT_EQ(option.connection_type, EConnectionType::CONNECTION_TYPE_POOLED);
    ASSERT_EQ(option.protocol, EProtocolType::PROTOCOL_HTTP);
    ASSERT_EQ(option.connect_timeout_ms, SOCKET_CONNECT_TIMEOUT_MS);
    ASSERT_EQ(option.timeout_ms, SOCKET_TIMEOUT_MS);
    ASSERT_EQ(option.max_retry, SOCKET_MAX_RETRY);

    std::thread td([&channel](){
        ChannelUnitTest channel_test(channel);
        ASSERT_TRUE(!!channel_test.socket());
    });

    td.join();

    globalDestroy();
}

TEST(ChannelTest, session) {
    Channel channel;
    ASSERT_EQ(channel.status(), ERpcStatus::RPC_STATUS_INIT);
    Session session;
    session.sync();
    ASSERT_TRUE(session.failed());
    ASSERT_EQ(session.getErrText(), "protocol not found");
    session.async([](ESessionError, ResponseBase*){});
    ASSERT_TRUE(session.failed());
    session.reset();
    channel.init("127.0.0.1:1234", nullptr);
    session.to(channel);
    session.sync();
    ASSERT_TRUE(session.failed());
    session.reset();
    Channel channel1;
    channel1.init("127.0.0.1:12200", nullptr);
    session.to(channel1);
    session.sync();
    ASSERT_TRUE(session.failed());

    PipelineSession session1;
    session1.sync();
    ASSERT_FALSE(session1.failed());
    HttpRequest req;
    HttpResponse rsp;
    session1.pipe(req, rsp);
    session1.sync();
    ASSERT_TRUE(session1.failed());
}
