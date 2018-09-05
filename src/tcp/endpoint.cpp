// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/2.
//

#include "tcp/endpoint.h"
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <netdb.h>
#include "common/utils.h"

namespace antflash {

bool EndPoint::parseFromString(const char *str) {
    constexpr auto STR_MAX_SIZE = 64;
    char inner_str[STR_MAX_SIZE];
    const char* p = str;
    size_t i = 0;
    while (':' != *p) {
        inner_str[i++] = *p;
        if (i >= STR_MAX_SIZE || '\0' == *p) {
            return false;
        }
        ++p;
    }

    inner_str[i] = '\0';
    //Utils::trim(inner_str);

    //try parse ip
    int rc = inet_pton(AF_INET, inner_str, &ip);
    if (rc <= 0) {
        //try parse host name
        struct hostent *result = nullptr;
#if defined(OS_MACOSX)
        result = gethostbyname(inner_str);
#else
        char buf[1024];
        int error = 0;
        struct hostent ent;
        if (gethostbyname_r(inner_str, &ent, buf, sizeof(buf),
                        &result, &error) != 0) {
            return false;
        }
#endif // defined(__APPLE__)
        if (nullptr == result) {
            return false;
        }
        memmove(&ip, result->h_addr, result->h_length);
    }

    ++p; //skip ':'
    char* end = nullptr;
    port = std::strtol(p, &end, 10);
    if (end == p || *end) {
        return false;
    }
    if (port < 0 || port > 65535) {
        return false;
    }

    return true;
}

std::string EndPoint::ipToStr() const {
    char tmp_ip[INET_ADDRSTRLEN];
    char tmp[INET_ADDRSTRLEN + 16];
    inet_ntop(AF_INET, &ip, tmp_ip, INET_ADDRSTRLEN);
    sprintf(tmp, "%s:%d", tmp_ip, port);
    return std::string(tmp);
}

}
