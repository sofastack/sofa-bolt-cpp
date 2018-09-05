#include <iostream>
#include <stdio.h>
#include <thread>
#include "rpc.h"
#include "sample.pb.h"
#include "../src/common/log.h"

int main() {
    if (!antflash::globalInit()) {
        LONG_ERROR("init fail.");
        return -1;
    }

    antflash::Channel channel;
    if (!channel.init("127.0.0.1:12200", nullptr)) {
        LONG_ERROR("channel init fail.");
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
    session.send(request).to(channel).receiveTo(response).async(
            [&sub_rsp](antflash::ESessionError err, antflash::ResponseBase* rsp) {
                if (err != antflash::ESessionError::SESSION_OK) {
                    LOG_INFO("get response fail:{}", antflash::Session::getErrText(err));
                    return;
                }
                LOG_INFO("response:{}", sub_rsp.result());
            }
            );

    std::this_thread::sleep_for(std::chrono::seconds(1));

    antflash::globalDestroy();
    
    return 0;
}

