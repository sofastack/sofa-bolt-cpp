#include <iostream>
#include <stdio.h>
#include <thread>
#include "rpc.h"
#include "sample.pb.h"
#include "../src/common/log.h"

int main() {
    if (!antflash::globalInit()) {
        LOG_ERROR("init fail.");
        return -1;
    }

    antflash::Channel httpChannel;
    antflash::ChannelOptions httpOptions;
    httpOptions.protocol = antflash::EProtocolType::PROTOCOL_HTTP;
    //if (!channel.init("127.0.0.1:8888", &options)) {
    if (!httpChannel.init("127.0.0.1:13330", &httpOptions)) {
        LOG_ERROR("httpChannel init fail.");
        return -1;
    }

    {
        antflash::HttpRequest applicationHttpRequest;
        antflash::HttpResponse applicationHttpResponse;
        //request.uri("127.0.0.1:8888/configs/application")
        applicationHttpRequest.uri("127.0.0.1:13330/configs/application")
                .method(antflash::EHttpMethod::HTTP_METHOD_POST)
                .attach_type("application/json")
                .attach("{\"DataCenter\":\"dc\",\"AppName\":\"testApp\",\"AntShareCloud\":true}");
        antflash::Session applicationHttpsession;
        applicationHttpsession.send(applicationHttpRequest).to(httpChannel).receiveTo(applicationHttpResponse).sync();

        if (applicationHttpsession.failed()) {
            LOG_INFO("get response fail");
        } else {
            LOG_INFO("response body: {}", applicationHttpResponse.body());
        }
    }

    {
        antflash::HttpRequest subscribeHttpRequest;
        antflash::HttpResponse subscribeHttpResponse;
        //request.uri("127.0.0.1:8888/services/subscribe")
        subscribeHttpRequest.uri("127.0.0.1:13330/services/subscribe")
                .method(antflash::EHttpMethod::HTTP_METHOD_POST)
                .attach_type("application/json")
                .attach("{\"serviceName\":\"com.alipay.rpc.common.service.facade.pb.SampleServicePb:1.0\"}");
        antflash::Session subscribeHttpSession;
        subscribeHttpSession.send(subscribeHttpRequest).to(httpChannel).receiveTo(subscribeHttpResponse).sync();

        if (subscribeHttpSession.failed()) {
            LOG_INFO("get response fail");
        } else {
            LOG_INFO("response body: {}", subscribeHttpResponse.body());
        }
    }



    antflash::Channel rpcChannel;
    antflash::ChannelOptions rpcOptions;
    rpcOptions.connection_type = antflash::EConnectionType::CONNECTION_TYPE_POOLED;
    if (!rpcChannel.init("127.0.0.1:12220", &rpcOptions)) {
        printf("%s", "rpcOptions init fail.");
        return -1;
    }

    antflash::BoltRequest boltRequest;
    SampleServicePbRequest sub_req;

    SampleServicePbResult sub_rsp;
    antflash::BoltResponse boltResponse(sub_rsp);

    while (true) {
        sub_req.set_name("alipay");
        boltRequest.service("com.alipay.rpc.common.service.facade.pb.SampleServicePb:1.0")
                .method("hello").data(sub_req);
        antflash::Session rpcSession;
        rpcSession.send(boltRequest).to(rpcChannel).receiveTo(boltResponse).sync();
        boltResponse.isHeartbeat();

        if (rpcSession.failed()) {
            LOG_INFO("get response fail {}", rpcSession.getErrText());
        } else {
            if (boltResponse.status() != antflash::BoltResponse::SUCCESS) {
                LOG_INFO("get response fail, response status:{}", boltResponse.status());
            } else {
                LOG_INFO("get response {}, response:{}",
                         rpcSession.getErrText(), sub_rsp.result());
            }
        }
        rpcSession.reset();

        std::this_thread::sleep_for(
                std::chrono::seconds(3));
    }

}

