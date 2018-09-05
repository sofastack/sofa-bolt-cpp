// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/25.
//

#include "common/uri.h"
#include <gtest/gtest.h>

using namespace antflash;

TEST(URITest, baseFunction) {
    {
        antflash::URI uri;
        ASSERT_TRUE(uri.parse_ok());
    }
    {
        antflash::URI uri("https://127.0.0.1:8080/test");
        ASSERT_TRUE(uri.parse_ok());
        ASSERT_EQ(uri.path(), "/test");
        ASSERT_EQ(uri.host(), "127.0.0.1");
        ASSERT_EQ(uri.port(), 8080);
        uri = "https://127.0.0.1:8000/test2";
        ASSERT_TRUE(uri.parse_ok());
        ASSERT_EQ(uri.path(), "/test2");
        ASSERT_EQ(uri.host(), "127.0.0.1");
        ASSERT_EQ(uri.port(), 8000);
    }
    {
        std::string str("https://127.0.0.1:8080/test?  abc ");
        antflash::URI uri(str);
        ASSERT_FALSE(uri.parse_ok());
        ASSERT_EQ(uri.path(), "/test");
        ASSERT_EQ(uri.host(), "127.0.0.1");
        ASSERT_EQ(uri.port(), 8080);
        str = "https://127.0.0.1:8000/test";
        uri = str;
        ASSERT_TRUE(uri.parse_ok());
        ASSERT_EQ(uri.path(), "/test");
        ASSERT_EQ(uri.host(), "127.0.0.1");
        ASSERT_EQ(uri.port(), 8000);
    }

    {
        std::string str("https://127.0.0.1:8080/test?  abc ");
        antflash::URI uri(str);
        ASSERT_FALSE(uri.parse_ok());
        ASSERT_EQ(uri.path(), "/test");
        ASSERT_EQ(uri.host(), "127.0.0.1");
        ASSERT_EQ(uri.port(), 8080);
        str = "https://127.0.0.1:8000/test";
        uri = str;
        ASSERT_TRUE(uri.parse_ok());
        ASSERT_EQ(uri.path(), "/test");
        ASSERT_EQ(uri.host(), "127.0.0.1");
        ASSERT_EQ(uri.port(), 8000);
    }

    {
        antflash::URI uri("mailto:xxx@xxx.xx");
        antflash::URI uri1("mailto:xxx@xxx.xx@yyy.yy");
        antflash::URI uri2("     mailto:xxx@xxx.xx");
        antflash::URI uri3("     m    ");
        antflash::URI uri4("https://127.0.0.1:8080/test?user#a");
        antflash::URI uri5("https://127.0.0.1:8080/test?user#a   ");
        antflash::URI uri6("https://127.0.0.1:8080/test?user#a   b");
    }
}
