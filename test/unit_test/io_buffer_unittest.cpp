// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/23.
//

#include "common/io_buffer.h"
#include <string>
#include <gtest/gtest.h>
#include <fstream>
#include <fcntl.h>
#include <thread>
#include <random>
#include <google/protobuf/message.h>
#include <common/common_defines.h>
#include "io_buffer_unittest.pb.h"

using namespace antflash;

TEST(IOBufferTest, baseFunction) {
    IOBuffer one;
    std::string ss_one = "Construct";
    const std::string& const_ss_one = ss_one;
    one.append(const_ss_one);

    ASSERT_EQ(one.slice_num(), 1U);
    auto p = one.slice(0);
    auto p_copy(p);
    auto p_move(std::move(p_copy));

    IOBuffer two(one);
    ASSERT_STREQ(one.to_string().c_str(), ss_one.c_str());
    ASSERT_STREQ(two.to_string().c_str(), ss_one.c_str());
    IOBuffer three(std::move(one));
    ASSERT_STREQ(three.to_string().c_str(), ss_one.c_str());
    ASSERT_EQ(one.empty(), true);
    one = two;
    ASSERT_STREQ(one.to_string().c_str(), ss_one.c_str());
    ASSERT_STREQ(two.to_string().c_str(), ss_one.c_str());
    one = std::move(two);
    ASSERT_STREQ(one.to_string().c_str(), ss_one.c_str());
    ASSERT_EQ(two.empty(), true);
    two.swap(three);
    ASSERT_STREQ(two.to_string().c_str(), ss_one.c_str());
    ASSERT_EQ(three.empty(), true);

    std::thread work([]() {
        IOBuffer buffer;
        ASSERT_EQ(buffer.empty(), true);
        ASSERT_EQ(buffer.slice_num(), 0U);

        size_t block_size = 0;
        std::string ss("Hello World");
        {
            buffer.append(ss.c_str());
            ASSERT_EQ(buffer.empty(), false);
            ASSERT_EQ(buffer.length(), ss.length());
            ASSERT_STREQ(buffer.to_string().c_str(), ss.c_str());
            ASSERT_EQ(buffer.slice_num(), 1U);
            auto p = buffer.slice(0);
            ASSERT_EQ(p.second, ss.length());
            std::string slice_data(p.first, p.second);
            ASSERT_STREQ(slice_data.c_str(), ss.c_str());
            block_size += ss.length();
        }

        {
            buffer.append(ss);
            ASSERT_EQ(buffer.empty(), false);
            ASSERT_EQ(buffer.length(), ss.length() * 2);
            ASSERT_STREQ(buffer.to_string().c_str(), (ss + ss).c_str());
            ASSERT_EQ(buffer.slice_num(), 1U);
            auto p0 = buffer.slice(0);
            ASSERT_EQ(p0.second, ss.length() * 2);
            std::string sp0(p0.first, p0.second);
            ASSERT_STREQ(sp0.c_str(), (ss + ss).c_str());
            auto b0 = buffer.block(0);
            block_size += ss.length();
            ASSERT_EQ(b0.second, block_size);
        }

        {
            ss.clear();
            for (size_t i = 0; i < BUFFER_DEFAULT_BLOCK_SIZE - 100; ++i) {
                ss.append("x");
            }
            ss.append("Hello World");
            buffer.clear();
            buffer.append(ss);

            ASSERT_EQ(buffer.empty(), false);
            ASSERT_EQ(buffer.length(), ss.length());

            ASSERT_STREQ(buffer.to_string().c_str(), ss.c_str());
            ASSERT_EQ(buffer.slice_num(), 1U);
            auto p0 = buffer.slice(0);
            ASSERT_EQ(p0.second, ss.length());
            std::string sp0(p0.first, p0.second);
            ASSERT_STREQ(sp0.c_str(), ss.c_str());

            block_size += ss.length();
            auto b0 = buffer.block(0);
            ASSERT_EQ(b0.second, block_size);
        }

        {
            ss.clear();
            for (size_t i = 0; i < 100; ++i) {
                ss.append("x");
            }
            ss.append("Hello World");
            buffer.clear();
            buffer.append(ss);

            ASSERT_EQ(buffer.empty(), false);
            ASSERT_EQ(buffer.length(), ss.length());

            ASSERT_STREQ(buffer.to_string().c_str(), ss.c_str());
            ASSERT_EQ(buffer.slice_num(), 2U);
            auto p0 = buffer.slice(0);
            auto p1 = buffer.slice(1);
            ASSERT_EQ(p0.second + p1.second, ss.length());

            block_size += p0.second;
            auto b0 = buffer.block(0);
            auto b1 = buffer.block(1);
            ASSERT_EQ(b0.second, block_size);
            ASSERT_EQ(b1.second, p1.second);

            IOBuffer::BlockUnitTest wb0 = buffer.weakBlock(0);
            IOBuffer::BlockUnitTest wb1 = buffer.weakBlock(1);
            ASSERT_TRUE(wb0.exist());
            ASSERT_TRUE(wb1.exist());
            buffer.clear();
            ASSERT_FALSE(wb0.exist());
            ASSERT_TRUE(wb1.exist());
        }
    });
    work.join();
}

TEST(IOBufferTest, fdFunction) {
    std::thread work([]() {
        IOBuffer buffer;
        size_t block_size = 0;

        std::string ss("I'm so missing my dear darling now");
        buffer.append(ss);

        block_size += ss.length();

        int fd = open("IOBufferTest", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        ASSERT_GE(fd, 0);
        ASSERT_EQ(buffer.length(), ss.length());
        ASSERT_STREQ(buffer.to_string().c_str(), ss.c_str());
        auto nr = buffer.cut_into_file_descriptor(fd, ss.length());
        close(fd);

        ASSERT_EQ(nr, (ssize_t)ss.length());
        ASSERT_EQ(buffer.length(), 0UL);
        ASSERT_STREQ(buffer.to_string().c_str(), "");

        ASSERT_EQ(buffer.slice_num(), 0U);

        fd = open("IOBufferTest", O_RDONLY);
        ASSERT_GE(fd, 0);
        constexpr size_t MIN_ONCE_READ = 4096;
        nr = buffer.append_from_file_descriptor(fd, MIN_ONCE_READ);
        ASSERT_EQ(nr, (ssize_t)ss.length());
        ASSERT_EQ(buffer.length(), ss.length());
        ASSERT_STREQ(buffer.to_string().c_str(), ss.c_str());

        block_size += ss.length();
        auto b1 = buffer.block(0);
        ASSERT_EQ(b1.second, block_size);

        ASSERT_EQ(buffer.slice_num(), 1U);
        auto p0 = buffer.slice(0);
        ASSERT_EQ(p0.second, ss.length());
        std::string sp0(p0.first, p0.second);
        ASSERT_STREQ(sp0.c_str(), ss.c_str());
        close(fd);

        //large fd file
        std::string checkY;
        for (size_t i = 0; i < BUFFER_DEFAULT_BLOCK_SIZE; ++i) {
            buffer.append('y');
            checkY.append("y");
        }
        buffer.append(ss);

        size_t buffer_len = BUFFER_DEFAULT_BLOCK_SIZE + ss.length() * 2;

        ASSERT_EQ(buffer.slice_num(), 2U);

        IOBuffer::BlockUnitTest wb0 = buffer.weakBlock(0);
        IOBuffer::BlockUnitTest wb1 = buffer.weakBlock(1);
        ASSERT_TRUE(wb0.exist());
        ASSERT_TRUE(wb1.exist());

        ASSERT_EQ(buffer.length(), buffer_len);
        fd = open("IOBufferTest_Big", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        ASSERT_GE(fd, 0);

        size_t cur_size = 0;
        constexpr size_t MIN_ONCE_WRITE = 1024;
        while (cur_size < buffer_len) {
            auto nr = buffer.cut_into_file_descriptor(fd, MIN_ONCE_WRITE);
            ASSERT_LE(nr, (ssize_t)buffer_len);
            ASSERT_GE(nr, 0);
            cur_size += nr;
        }
        ASSERT_EQ(cur_size, buffer_len);
        ASSERT_EQ(buffer.length(), 0UL);
        ASSERT_STREQ(buffer.to_string().c_str(), "");

        close(fd);

        fd = open("IOBufferTest_Big", O_RDONLY);
        ASSERT_GE(fd, 0);
        cur_size = 0;
        while (cur_size < buffer_len) {
            nr = buffer.append_from_file_descriptor(fd, MIN_ONCE_READ);
            ASSERT_LE(nr, (ssize_t)MIN_ONCE_READ);
            ASSERT_GE(nr, 0);
            cur_size += nr;
        }
        ASSERT_EQ(cur_size, buffer_len);
        ASSERT_EQ(buffer.length(), buffer_len);
        std::string buf_str = buffer.to_string();

        ASSERT_STREQ(buf_str.substr(0, ss.length()).c_str(), ss.c_str());
        ASSERT_STREQ(buf_str.substr(ss.length(), checkY.length()).c_str(),
                     checkY.c_str());
        ASSERT_STREQ(buf_str.substr(ss.length() + checkY.length(), ss.length()).c_str(),
                     ss.c_str());
        close(fd);

    });
    work.join();
}

TEST(IOBufferTest, constructions) {
    std::thread work([]() {
        IOBuffer buffer1;
        std::string s1("I'm so missing my dear darling now");
        std::string s2(s1);
        std::string smid;
        for (size_t i = 0; i < BUFFER_DEFAULT_BLOCK_SIZE; ++i) {
            s2.append("L");
            smid.append("L");
        }
        std::string s3(s2);
        s3.append(s1);
        buffer1.append(s3);

        ASSERT_EQ(buffer1.length(), s3.length());
        ASSERT_STREQ(buffer1.to_string().c_str(), s3.c_str());

        ASSERT_EQ(buffer1.slice_num(), 2U);
        auto b10 = buffer1.block(0);
        auto b11 = buffer1.block(1);
        auto block_size1 = b10.second + b11.second;
        ASSERT_EQ(s3.length(), block_size1);

        IOBuffer buffer2(buffer1);
        ASSERT_EQ(buffer2.length(), s3.length());
        ASSERT_STREQ(buffer2.to_string().c_str(), s3.c_str());

        ASSERT_EQ(buffer2.slice_num(), 2U);
        auto b20 = buffer2.block(0);
        auto b21 = buffer2.block(1);
        auto block_size2 = b10.second + b11.second;
        ASSERT_EQ(s3.length(), block_size2);

        size_t pop_size = buffer2.pop_back(s1.length());
        ASSERT_EQ(pop_size, s1.length());
        ASSERT_EQ(buffer1.length(), s3.length());
        ASSERT_EQ(buffer2.length(), s2.length());
        b20 = buffer2.block(0);
        b21 = buffer2.block(1);
        block_size2 = b10.second + b11.second;
        ASSERT_EQ(s3.length(), block_size2);

        auto s11 = buffer1.slice(1);
        auto s21 = buffer2.slice(1);
        ASSERT_EQ(s1.length(), s11.second - s21.second);

        IOBuffer buffer3(std::move(buffer2));
        ASSERT_EQ(buffer3.length(), s2.length());
        ASSERT_EQ(buffer2.length(), 0UL);

        buffer2 = buffer3;
        ASSERT_EQ(buffer3.length(), s2.length());
        ASSERT_EQ(buffer2.length(), s2.length());
        buffer3.pop_front(10);
        buffer3.pop_front(s1.length() - 10);
        ASSERT_EQ(buffer3.length(), smid.length());
        ASSERT_STREQ(buffer3.to_string().c_str(), smid.c_str());
        ASSERT_EQ(buffer2.length(), s2.length());
        ASSERT_STREQ(buffer2.to_string().c_str(), s2.c_str());

        buffer2 = std::move(buffer3);
        ASSERT_EQ(buffer3.length(), 0UL);
        ASSERT_EQ(buffer2.length(), smid.length());
        ASSERT_STREQ(buffer2.to_string().c_str(), smid.c_str());

        buffer3.append(s1);
        buffer2.clear();
        buffer2.append(std::move(buffer3));
        ASSERT_EQ(buffer3.length(), 0UL);
        ASSERT_EQ(buffer2.length(), s1.length());
        buffer3.append(smid);
        buffer2.append(buffer3);
        ASSERT_EQ(buffer3.length(), smid.length());
        ASSERT_STREQ(buffer3.to_string().c_str(), smid.c_str());
        ASSERT_EQ(buffer2.length(), s2.length());
        ASSERT_STREQ(buffer2.to_string().c_str(), s2.c_str());

        buffer1.cut(&buffer2, s1.length());
        ASSERT_EQ(buffer1.length(), s3.length() - s1.length());
        ASSERT_STREQ(buffer2.to_string().c_str(), s3.c_str());
        ASSERT_STREQ(buffer1.to_string().c_str(), (smid + s1).c_str());
        std::string cuts;
        buffer1.cut(cuts, 0);
        ASSERT_EQ(buffer1.length(), s3.length() - s1.length());
        buffer1.cut(cuts, 12);
        ASSERT_EQ(buffer1.length(), s3.length() - s1.length() - 12);
        buffer1.cut(cuts, smid.length() - 12);
        ASSERT_EQ(buffer1.length(), s3.length() - s1.length() - smid.length());
        ASSERT_STREQ(buffer1.to_string().c_str(), s1.c_str());
        ASSERT_STREQ(cuts.c_str(), smid.c_str());


        char copyto[128] = {0,};
        buffer1.copy_to(copyto, 8);
        ASSERT_STREQ(buffer1.to_string().c_str(), s1.c_str());
        ASSERT_EQ(buffer1.length(), s1.length());
        ASSERT_STREQ(copyto, s1.substr(0, 8).c_str());
        buffer1.copy_to(copyto, s1.length());
        ASSERT_STREQ(buffer1.to_string().c_str(), s1.c_str());
        ASSERT_STREQ(copyto, s1.c_str());
        buffer1.pop_back(s1.length());
        ASSERT_EQ(buffer1.length(), 0UL);

        buffer1.append(nullptr, 10);
        buffer1.append(copyto, 0);

        buffer1.clear();
        buffer1.append(s3);
        ASSERT_EQ(buffer1.pop_back(1000000), s3.length());
        ASSERT_EQ(buffer1.length(), 0UL);
        buffer1.append(s3);
        ASSERT_EQ(buffer1.cut(&buffer2, 1000000), s3.length());
        ASSERT_EQ(buffer1.length(), 0UL);
        ASSERT_EQ(buffer1.slice_num(), 0U);

        buffer1.append(s1);
        ASSERT_STREQ(buffer1.to_string().c_str(), s1.c_str());
        buffer1.copy_to(copyto, 128);
        ASSERT_STREQ(buffer1.to_string().c_str(), s1.c_str());
        ASSERT_STREQ(copyto, s1.c_str());

    });
    work.join();
}

TEST(IOBufferTest, multiThread) {
    IOBuffer buffer;
    auto dest_time = std::chrono::system_clock::now()
                     + std::chrono::milliseconds(100);
    std::thread t1([&buffer, &dest_time]() {
        std::this_thread::sleep_until(dest_time);
        std::string ss("I'm so missing my dear darling now");
        buffer.append(ss);

        ASSERT_EQ(buffer.empty(), false);
        ASSERT_EQ(buffer.length(), ss.length());

        ASSERT_STREQ(buffer.to_string().c_str(), ss.c_str());
        ASSERT_EQ(buffer.slice_num(), 1U);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ASSERT_EQ(buffer.slice_num(), 2U);
    });

    std::thread t2([&buffer, &dest_time]() {
        std::this_thread::sleep_until(dest_time);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::string ss("I'm so missing my dear darling now");
        buffer.append(ss);
        ASSERT_EQ(buffer.empty(), false);
        ASSERT_EQ(buffer.length(), ss.length() * 2);

        ASSERT_STREQ(buffer.to_string().c_str(), (ss + ss).c_str());
    });

    t1.join();
    t2.join();

    ASSERT_EQ(buffer.slice_num(), 2U);
}

TEST(IOBufferTest, ZeroStream) {
    std::thread work([]() {
        UnitTestProto proto1;
        proto1.set_name("UnitTest");
        proto1.set_id(12345);
        proto1.set_uid(54321);
        proto1.mutable_msg()->add_msg("success");
        proto1.mutable_msg()->add_code(200);

        std::string msg;
        std::random_device rd;
        for (size_t i = 0; i < 20000; ++i) {
            if (i % 2 == 0) {
                char c = rd() % 94 + 33;
                msg.append(1, c);
            }
        }
        proto1.mutable_msg()->add_msg(msg);
        proto1.mutable_msg()->add_code(1024);

        IOBuffer buffer;
        IOBufferZeroCopyOutputStream wrapper(&buffer);
        ASSERT_TRUE(proto1.SerializeToZeroCopyStream(&wrapper));
        ASSERT_GT(wrapper.ByteCount(), 0);
        ASSERT_EQ(proto1.SerializeAsString().length(), buffer.length());
        ASSERT_EQ(wrapper.ByteCount(), (ssize_t)buffer.length());
        buffer.dump();

        IOBuffer buffer2;
        buffer2.swap(buffer);

        UnitTestProto proto2;
        IOBufferZeroCopyInputStream stream(buffer2);
        google::protobuf::io::ZeroCopyInputStream *input = &stream;
        google::protobuf::io::CodedInputStream decoder(input);
        ASSERT_TRUE(proto2.ParseFromCodedStream(&decoder));

        ASSERT_STREQ(proto1.name().c_str(), proto2.name().c_str());
        ASSERT_EQ(proto1.id(), proto2.id());
        ASSERT_EQ(proto1.uid(), proto2.uid());
        ASSERT_EQ(proto1.msg().msg_size(), proto2.msg().msg_size());
        ASSERT_EQ(proto1.msg().code_size(), proto2.msg().code_size());
        for (int i = 0; i < proto1.msg().msg_size(); ++i) {
            ASSERT_STREQ(proto1.msg().msg(i).c_str(), proto2.msg().msg(i).c_str());
        }
        for (int i = 0; i < proto1.msg().code_size(); ++i) {
            ASSERT_EQ(proto1.msg().code(i), proto2.msg().code(i));
        }
    });

    work.join();
}
