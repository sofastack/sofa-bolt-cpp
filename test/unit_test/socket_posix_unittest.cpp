// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/25.
//

#include "tcp/socket_base.h"
#include <gtest/gtest.h>
#include <fcntl.h>

TEST(SocketPosixTest, base) {
    ASSERT_FALSE(antflash::base::set_non_blocking(-1));
    ASSERT_FALSE(antflash::base::set_blocking(-1));
    int fd = open("socket_posix", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    ASSERT_TRUE(antflash::base::set_non_blocking(fd));
    ASSERT_TRUE(antflash::base::set_non_blocking(fd));
    ASSERT_TRUE(antflash::base::set_blocking(fd));
    ASSERT_TRUE(antflash::base::set_blocking(fd));

    antflash::base::FdGuard fd_guard(-1);
    ASSERT_FALSE(antflash::base::connected(fd_guard));
    antflash::base::FdGuard fd_guard2(fd);
    ASSERT_FALSE(antflash::base::connected(fd_guard2));
}
