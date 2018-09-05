// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/17.
//

#ifndef RPC_COMMON_URI_H
#define RPC_COMMON_URI_H

#include <string>

namespace antflash {

class URI {
public:
    URI() : _parse_ok(true) {}
    URI(const std::string& url) : _parse_ok(true) {
        parseFromString(url);
    }
    URI(const char* url) : _parse_ok(true) {
        parseFromString(url);
    }
    ~URI() {}

    void parseFromString(const char*);
    void parseFromString(const std::string& url) {
        parseFromString(url.c_str());
    }

    URI& operator=(const std::string& url) {
        parseFromString(url);
        return *this;
    }
    URI& operator=(const char* url) {
        parseFromString(url);
        return *this;
    }

    const std::string& path() const {
        return _path;
    }

    const std::string& host() const {
        return _host;
    }

    int port() const {
        return _port;
    }

    void clear() {
        _schema.clear();
        _host.clear();
        _path.clear();
        _user_info.clear();
        _port = -1;
        _parse_ok = true;
    }

    bool parse_ok() const {
        return _parse_ok;
    }

private:
    std::string _schema;
    std::string _host;
    std::string _path;
    std::string _user_info;
    std::string _query;
    std::string _fragment;
    int _port;
    bool _parse_ok;
};


}

#endif //RPC_COMMON_URI_H
