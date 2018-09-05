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

    antflash::Channel channel;
    antflash::ChannelOptions options;
    options.connection_type = antflash::EConnectionType::CONNECTION_TYPE_POOLED;
    if (!channel.init("bar1.inc.alipay.net:12200", &options)) {
        printf("%s", "channel init fail.");
        return -1;
    }

    antflash::BoltRequest request;
    SampleServicePbRequest sub_req;

    SampleServicePbResult sub_rsp;
    antflash::BoltResponse response(sub_rsp);

    sub_req.set_name("zhenggu");
    request.service("com.alipay.rpc.common.service.facade.pb.SampleServicePb:1.0")
            .method("hello").data(sub_req);
    antflash::Session session;
    session.send(request).to(channel).receiveTo(response).sync();
    response.isHeartbeat();

    if (session.failed()) {
        LOG_INFO("get response fail {}", session.getErrText());
    } else {
        if (response.status() != antflash::BoltResponse::SUCCESS) {
            LOG_INFO("get response fail, response status:{}", response.status());
        } else {
            LOG_INFO("get response {}, response:{}",
                      session.getErrText(), sub_rsp.result());
        }
    }

    session.reset();

    antflash::PipelineSession pipe_session;
    constexpr size_t pipe_size = 10;
    pipe_session.reserve(pipe_size);
    SampleServicePbResult sub_rsps[pipe_size];
    std::vector<antflash::BoltResponse> responses;
    responses.reserve(pipe_size);
    for (size_t i = 0; i < pipe_size; ++i) {
        responses.emplace_back(sub_rsps[i]);
        pipe_session.pipe(request, responses[i]);
    }
    pipe_session.to(channel).sync();

    if (pipe_session.failed()) {
        LOG_INFO("get response fail {}", pipe_session.getErrText());
    } else {
        for (size_t i = 0; i < pipe_size; ++i) {
            LOG_INFO("response:{}", sub_rsps[i].result());
        }
    }

    std::this_thread::sleep_for(
            std::chrono::seconds(20));

    antflash::globalDestroy();
    
    return 0;
}

