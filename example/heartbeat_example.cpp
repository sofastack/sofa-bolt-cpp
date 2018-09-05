#include <iostream>
#include <stdio.h>
#include <thread>
#include <random>
#include <thread>
#include "rpc.h"
#include "../src/common/log.h"

int main() {
    if (!antflash::globalInit()) {
        LOG_INFO("global init fail.");
        return -1;
    }

    antflash::ChannelOptions options;
    options.connection_type =
            antflash::EConnectionType::CONNECTION_TYPE_POOLED;
    options.max_retry = 1;
    antflash::Channel channel;
    if (!channel.init("127.0.0.1:12200", &options)) {
        LOG_INFO("channel init fail.");
        return -1;
    }

    antflash::BoltRequest request;
    std::vector<std::unique_ptr<std::thread>> threads;
    std::vector<bool> exits;
    //size_t total_size = std::thread::hardware_concurrency();
    size_t total_size = 3;
    for (size_t i = 0; i < total_size; ++i) {
        exits.push_back(false);
        threads.emplace_back(
        new std::thread([&channel, &request, &exits, i]() {
            std::default_random_engine e;
            std::uniform_int_distribution<> u(1, 20);
            while (!exits[i]) {
                antflash::BoltResponse response;
                antflash::Session session;
                LOG_INFO("thread[{}] begin heartbeat.", i);
                session.send(request).to(channel).receiveTo(response).sync();

                if (session.failed()) {
                    LOG_INFO("thread[{}] session fail: {}", i, session.getErrText());
                } else if (response.isHeartbeat()) {
                    LOG_INFO("thread[{}] session success, status[{}]", i, response.status());
                }

                std::this_thread::sleep_for(
                        std::chrono::seconds(u(e)));
            }
        }));
    }

    std::this_thread::sleep_for(std::chrono::seconds(86400));
    for (size_t i = 0; i < total_size; ++i) {
        exits[i] = true;
    }

    for (size_t i = 0; i < total_size; ++i) {
        threads[i]->join();
    }

    antflash::globalDestroy();
    
    return 0;
}

