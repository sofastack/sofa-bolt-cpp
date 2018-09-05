// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/25.
//

#include "protocol/http/http_protocol.h"
#include "protocol/http/http_request.h"
#include "protocol/http/http_response.h"
#include "protocol/bolt/bolt_protocol.h"
#include "protocol/bolt/bolt_request.h"
#include "protocol/bolt/bolt_response.h"
#include "common/io_buffer.h"
#include <gtest/gtest.h>
#include <protocol/http/http_parser.h>

TEST(HttpTest, baseFunction) {
    {
        antflash::HttpRequest request;
        request.uri("127.0.0.1:12200")
                .method(antflash::HTTP_METHOD_POST)
                .attach("subscribe")
                .attach_type("application/json");
        ASSERT_TRUE(request.uri().parse_ok());
        ASSERT_EQ(request.attach_size(), 9UL);
        ASSERT_EQ(request.attach()->to_string(), std::string("subscribe"));
        ASSERT_EQ(request.attach_type(), std::string("application/json"));

        antflash::IOBuffer buffer;
        auto ret = antflash::http::assembleHttpRequest(request, 1, buffer);
        ASSERT_TRUE(ret);

        antflash::HttpRequest request2(request);
        std::string attach("unsubscribe");
        request2.attach(attach).attach_type(antflash::CONNECTION);
        ASSERT_EQ(request2.attach()->to_string(), attach);
        ASSERT_EQ(request2.attach_size(), attach.length());
        ASSERT_EQ(request2.attach_type(), std::string("connection"));
    }
    {
        antflash::HttpResponse response;
        antflash::IOBuffer buffer;
        ASSERT_EQ(response.deserialize(buffer),
                  antflash::EResParseResult::PARSE_ERROR);

        struct http_parser* parser = nullptr;
        void* p = parser;
        size_t unused = 0;
        antflash::HttpResponse::checkHeader(buffer, unused, unused, nullptr);
        antflash::HttpResponse::checkHeader(buffer, unused, unused, &p);
        antflash::HttpResponse::checkHeader(buffer, unused, unused, &p);

        response.setHttpParser((void*)parser);
        ASSERT_EQ(response.deserialize(buffer),
                  antflash::EResParseResult::PARSE_ERROR);
    }
    {
        antflash::HttpRequest request;
        request.uri("https://127.0.0.1:8080/test?  abc ")
                .method(antflash::HTTP_METHOD_POST)
                .attach("subscribe")
                .attach_type("application/json");
        antflash::IOBuffer buffer;
        auto ret = antflash::http::assembleHttpRequest(request, 1, buffer);
        ASSERT_FALSE(ret);
        antflash::http::assembleHeartbeatRequest(buffer);
        antflash::http::converseHttpRequest(10UL);
    }
    {
        antflash::IOBuffer buffer;
        std::string str("HTTP/1.1 200 OK\n"
                        "Content-Length: 61\n"
                        "Content-Type: application/json\n"
                        "Host: 127.0.0.1:13330\n"
                        "Accept: */*\n"
                        "User-Agent: curl/7.0\n"
                        "connection: keep-alive\n"
                        "log-id: 1\n"
                        "\n"
                        "{\"DataCenter\":\"dc\",\"AppName\":\"testApp\",\"AntShareCloud\":false}");
        buffer.append(str);
        antflash::HttpResponse response;
        struct http_parser* parser = nullptr;
        void* p = (void*)parser;
        size_t data_size = 0;
        size_t request_id = 0;
        ASSERT_EQ(antflash::http::parseHttpProtocol(buffer, data_size, request_id, &p),
                  antflash::EResParseResult::PARSE_OK);
        ASSERT_EQ(antflash::http::parseHttpResponse(response, buffer, p),
                  antflash::EResParseResult::PARSE_OK);

        ASSERT_EQ(data_size, 0UL);
        ASSERT_EQ(request_id, 0UL);
    }
    {
        antflash::IOBuffer buffer;
        std::string str("HTTP/1.1 200 OK\n"
                        "Content-Length: 61\n"
                        "Content-Type: application/json\n"
                        "Host: 127.0.0.1:13330\n"
                        "Accept: */*\n"
                        ": curl/7.0\n"
                        "connection: keep-alive"
                        "log-id: 1\n");

        buffer.append(str);
        antflash::HttpResponse response;
        struct http_parser* parser = nullptr;
        void* p = (void*)parser;
        size_t data_size = 0;
        size_t request_id = 0;
        ASSERT_EQ(antflash::http::parseHttpProtocol(buffer, data_size, request_id, &p),
                  antflash::EResParseResult::PARSE_ERROR);
        ASSERT_EQ(antflash::http::parseHttpResponse(response, buffer, p),
                  antflash::EResParseResult::PARSE_ERROR);

        ASSERT_EQ(data_size, 0UL);
        ASSERT_EQ(request_id, 0UL);
    }
}

TEST(BoltTest, baseFunction) {
    {
        antflash::BoltRequest request;
        antflash::IOBuffer buffer;
        request.data("test");
        auto ret = antflash::bolt::assembleBoltRequest(request, 1, buffer);
        ASSERT_TRUE(ret);

        std::string str("test2");
        request.data(str);
        ret = antflash::bolt::assembleBoltRequest(request, 1, buffer);
        ASSERT_TRUE(ret);

        std::shared_ptr<antflash::ResponseBase> response;
        ASSERT_FALSE(antflash::bolt::parseHeartbeatResponse(response));
        response = std::make_shared<antflash::BoltResponse>();
        ASSERT_FALSE(antflash::bolt::parseHeartbeatResponse(response));
    }
}
