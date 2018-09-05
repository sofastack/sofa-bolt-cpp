// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/3.
//

#include <tcp/endpoint.h>
#include <string>
#include <gtest/gtest.h>

TEST(EndPointTest, parseFromString) {
    std::string str = " I love u ";
    antflash::EndPoint point;
    GTEST_ASSERT_EQ(point.parseFromString(str.c_str()), false);
    str = "127.0.0.1:12345";
    GTEST_ASSERT_EQ(point.parseFromString(str.c_str()), true);
    GTEST_ASSERT_EQ(point.ip.s_addr, (unsigned int)16777343);
    GTEST_ASSERT_EQ(point.port, 12345);
    ASSERT_STREQ(str.c_str(), point.ipToStr().c_str());
    str = "288.2.1.500:12345";
    GTEST_ASSERT_EQ(point.parseFromString(str.c_str()), false);
    str = "127.0.0.1: 12345";
    GTEST_ASSERT_EQ(point.parseFromString(str.c_str()), true);
    str = " 127.0.0.1:12345";
    GTEST_ASSERT_EQ(point.parseFromString(str.c_str()), false);
    str = "127.0.0.1:12345 ";
    GTEST_ASSERT_EQ(point.parseFromString(str.c_str()), false);
    str = "127.0.0.1:70000";
    GTEST_ASSERT_EQ(point.parseFromString(str.c_str()), false);
    str = "127.0.0.1:0";
    GTEST_ASSERT_EQ(point.parseFromString(str.c_str()), true);
    GTEST_ASSERT_EQ(point.port, 0);
}
