#include <iostream>
#include <stdio.h>
#include "rpc.h"
#include "../src/common/log.h"

int main() {
    antflash::globalInit();

    antflash::Channel channel;
    antflash::ChannelOptions options;
    options.protocol = antflash::EProtocolType::PROTOCOL_HTTP;
    //if (!channel.init("127.0.0.1:8888", &options)) {
    if (!channel.init("13330", &options)) {
        LOG_ERROR("channel init fail.");
        return -1;
    }

    {
        antflash::HttpRequest request;
        antflash::HttpResponse response;
        //request.uri("127.0.0.1:8888/configs/application")
        request.uri("127.0.0.1:13330/configs/application")
                .method(antflash::EHttpMethod::HTTP_METHOD_POST)
                .attach_type("application/json")
                .attach("{\"DataCenter\":\"dc\",\"AppName\":\"testApp\",\"AntShareCloud\":false}");
        antflash::Session session;
        session.send(request).to(channel).receiveTo(response).sync();

        if (session.failed()) {
            LOG_INFO("get response fail");
        } else {
            LOG_INFO("response body: {}", response.body());
        }
    }

    {
        antflash::HttpRequest request;
        antflash::HttpResponse response;
        //request.uri("127.0.0.1:8888/services/subscribe")
        request.uri("127.0.0.1:13330/services/subscribe")
                .method(antflash::EHttpMethod::HTTP_METHOD_POST)
                .attach_type("application/json")
                .attach("{\"serviceName\":\"com.alipay.rpc.common.service.facade.pb.SampleServicePb:1.0@DEFAULT\"}");
        antflash::Session session;
        session.send(request).to(channel).receiveTo(response).sync();

        if (session.failed()) {
            LOG_INFO("get response fail");
        } else {
            LOG_INFO("response body: {}", response.body());
        }
    }

    {
        antflash::HttpRequest request;
        antflash::HttpResponse response;
        //request.uri("127.0.0.1:8888/services/unsubscribe")
        request.uri("127.0.0.1:13330/services/unsubscribe")
                .method(antflash::EHttpMethod::HTTP_METHOD_POST)
                .attach_type("application/json")
                .attach("{\"serviceName\":\"com.alipay.rpc.common.service.facade.pb.SampleServicePb:1.0@DEFAULT\"}");
        antflash::Session session;
        session.send(request).to(channel).receiveTo(response).sync();

        if (session.failed()) {
            LOG_INFO("get response fail");
        } else {
            LOG_INFO("response body: {}", response.body());
        }
    }

    LOG_INFO("shut down");

    antflash::globalDestroy();
    
    return 0;
}

