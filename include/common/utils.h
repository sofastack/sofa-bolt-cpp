// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/2.
//

#ifndef RPC_COMMON_UTILS_H
#define RPC_COMMON_UTILS_H

#include <cctype>
#include <cstring>
#include <chrono>
#include <string>
#include <vector>
#include <experimental/string_view>

namespace antflash {

class Utils {
public:
    class Timer {
    public:
        Timer() : _begin(std::chrono::steady_clock::now()) {
        }

        ~Timer() {}

        void reset() {
            _begin = std::chrono::steady_clock::now();
        }

        size_t elapsed() {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - _begin).count();
        }

        size_t elapsedMicro() {
            return std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - _begin).count();
        }

        size_t elapsedSecond() {
            return std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - _begin).count();
        }

    private:
        std::chrono::steady_clock::time_point _begin;
    };

    static size_t getTimeStamp() {
        using namespace std::chrono;
        return duration_cast<seconds>(
                system_clock::now().time_since_epoch()).count();
    }

    static size_t getHighPrecisionTimeStamp() {
        using namespace std::chrono;
        return duration_cast<microseconds>(
                system_clock::now().time_since_epoch()).count();
    }

    static void leftTrim(char *str) {
        char* p = str;
        if (nullptr != p) {
            while (*p && std::isspace(*p)) {
                ++p;
            }
            if (p == str) {
                return;
            }
            size_t len = strlen(p);
            std::memmove(str, p, len);
            str[len] = '\0';
        }
    }

    static void rightTrim(char *str) {
        char *p = str;
        if (nullptr != p) {
            if ('\0' == *p) {
                return;
            }

            while (*(p + 1)) {
                ++p;
            }

            while (p != str && std::isspace(*p)) {
                *(p--) = '\0';
            }
        }
    }

    static void trim(char *str) {
        leftTrim(str);
        rightTrim(str);
    }

    static void trim(std::experimental::string_view& view) {
        //Left trim views
        size_t length = view.length();
        const char* data = view.data();
        while (std::isspace(*data)) {
            ++data;
            --length;
        }
        //Right trim
        const char* p = data + length;
        while (p != data && std::isspace(*--p)) {
            --length;
        }

        view = std::experimental::string_view(data, length);
    }

    static std::vector<std::string> stringSplit(
            const std::string& src, char delimiter) {
        std::vector<std::string> vec;
        std::string::size_type pos1, pos2;
        pos2 = src.find(delimiter);
        pos1 = 0;
        while (std::string::npos != pos2) {
            vec.emplace_back(src.substr(pos1, pos2 - pos1));

            pos1 = pos2 + 1;
            pos2 = src.find(delimiter, pos1);
        }
        if(pos1 != src.length()) {
            vec.emplace_back(src.substr(pos1));
        }

        return vec;
    }

    static std::vector<std::string> stringSplit(
            const std::string& src, const std::string& delimiter) {
        std::vector<std::string> vec;
        std::string::size_type pos1, pos2;
        pos2 = src.find(delimiter);
        pos1 = 0;
        while (std::string::npos != pos2) {
            vec.emplace_back(src.substr(pos1, pos2 - pos1));

            pos1 = pos2 + delimiter.length();
            pos2 = src.find(delimiter, pos1);
        }
        if(pos1 != src.length()) {
            vec.emplace_back(src.substr(pos1));
        }

        return vec;
    }

    static std::vector<std::experimental::string_view>
            stringSlice(const std::string& src, char delimiter) {
        std::vector<std::experimental::string_view> vec;
        std::string::size_type pos1, pos2;
        pos2 = src.find(delimiter);
        pos1 = 0;
        while (std::string::npos != pos2) {
            vec.emplace_back(&src[pos1], pos2 - pos1);

            pos1 = pos2 + 1;
            pos2 = src.find(delimiter, pos1);
        }
        if(pos1 != src.length()) {
            vec.emplace_back(&src[pos1], src.length() - pos1);
        }

        return vec;
    }

    static std::vector<std::experimental::string_view>
            stringSlice(const std::string& src, const std::string& delimiter) {
        std::vector<std::experimental::string_view> vec;
        std::string::size_type pos1, pos2;
        pos2 = src.find(delimiter);
        pos1 = 0;
        while (std::string::npos != pos2) {
            vec.emplace_back(&src[pos1], pos2 - pos1);

            pos1 = pos2 + delimiter.length();
            pos2 = src.find(delimiter, pos1);
        }
        if(pos1 != src.length()) {
            vec.emplace_back(&src[pos1], src.length() - pos1);
        }

        return vec;
    }

    static size_t splitNum(const std::string& src, char delimiter) {
        size_t num = 0;
        std::string::size_type pos1, pos2;
        pos2 = src.find(delimiter);
        pos1 = 0;
        while (std::string::npos != pos2) {
            ++num;
            pos1 = pos2 + 1;
            pos2 = src.find(delimiter, pos1);
        }
        if(pos1 != src.length()) {
            ++num;
        }

        return num;
    }

    static uint32_t MurmurHash2(const void* key, int len) {
        return MurmurHash2(key, len, 97);
    }

    static uint32_t MurmurHash2(const void* key, int len, uint32_t seed) {
        // 'm' and 'r' are mixing constants generated offline.
        // They're not really 'magic', they just happen to work well.

        const uint32_t m = 0x5bd1e995;
        const int r = 24;

        // Initialize the hash to a 'random' value
        uint32_t h = seed ^ len;

        // Mix 4 bytes at a time into the hash
        const unsigned char * data = (const unsigned char *)key;
        while(len >= 4) {
            uint32_t k = *(uint32_t *)data;

            k *= m;
            k ^= k >> r;
            k *= m;

            h *= m;
            h ^= k;

            data += 4;
            len -= 4;
        }

        // Handle the last few bytes of the input array
        const int8_t *idata = (const int8_t *)data;
        switch(len) {
            case 3: h ^= idata[2] << 16;
            case 2: h ^= idata[1] << 8;
            case 1: h ^= idata[0];
                h *= m;
        };

        // Do a few final mixes of the hash to ensure the last few
        // bytes are well-incorporated.
        h ^= h >> 13;
        h *= m;
        h ^= h >> 15;

        return h;
    }
};

}

#endif  //RPC_COMMON_UTILS_H
