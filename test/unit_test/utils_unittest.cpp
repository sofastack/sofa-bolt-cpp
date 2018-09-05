// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/3.
//

#include "common/utils.h"
#include <gtest/gtest.h>

TEST(UtilsTest, leftTrim) {
    char str1[64] = " I love u ";
    antflash::Utils::leftTrim(str1);
    ASSERT_STREQ(str1, "I love u ");
    char str2[64] = "  I love u ";
    antflash::Utils::leftTrim(str2);
    ASSERT_STREQ(str2, "I love u ");
    char str3[64] = "I love u ";
    antflash::Utils::leftTrim(str3);
    ASSERT_STREQ(str3, "I love u ");
    char str4[64] = "I love u";
    antflash::Utils::leftTrim(str4);
    ASSERT_STREQ(str4, "I love u");
    char str5[64] = "I love u  ";
    antflash::Utils::leftTrim(str5);
    ASSERT_STREQ(str5, "I love u  ");
    char str6[64] = "I  love  u";
    antflash::Utils::leftTrim(str6);
    ASSERT_STREQ(str6, "I  love  u");
    char str7[64] = "\0";
    antflash::Utils::leftTrim(str7);
    ASSERT_STREQ(str7, "\0");
    char* p = nullptr;
    antflash::Utils::leftTrim(p);
    ASSERT_STREQ(p, nullptr);
}

TEST(UtilsTest, rightTrim) {
    char str1[64] = " I love u ";
    antflash::Utils::rightTrim(str1);
    ASSERT_STREQ(str1, " I love u");
    char str2[64] = "  I love u ";
    antflash::Utils::rightTrim(str2);
    ASSERT_STREQ(str2, "  I love u");
    char str3[64] = "I love u ";
    antflash::Utils::rightTrim(str3);
    ASSERT_STREQ(str3, "I love u");
    char str4[64] = "I love u";
    antflash::Utils::rightTrim(str4);
    ASSERT_STREQ(str4, "I love u");
    char str5[64] = "I love u  ";
    antflash::Utils::rightTrim(str5);
    ASSERT_STREQ(str5, "I love u");
    char str6[64] = "I  love  u";
    antflash::Utils::rightTrim(str6);
    ASSERT_STREQ(str6, "I  love  u");
    char str7[64] = "\0";
    antflash::Utils::rightTrim(str7);
    ASSERT_STREQ(str7, "\0");
    char* p = nullptr;
    antflash::Utils::rightTrim(p);
    ASSERT_STREQ(p, nullptr);
}
