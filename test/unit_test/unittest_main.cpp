//
// Created by zhenggu.xwt on 18/4/3.
//
#include <gtest/gtest.h>
#include "../src/common/log.h"

int main(int argc, char** argv) {

    char* var = nullptr;
    var = getenv("ALOG_DEBUG");
    if (nullptr != var) {
        std::string level(var);
        if (level == "debug") {
            antflash::LogMessage::setLogLevel(
                    antflash::LogLevel::LOG_LEVEL_DEBUG);
        }
    }

    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

