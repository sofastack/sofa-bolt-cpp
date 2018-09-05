// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/17.
//

#include "common/uri.h"

namespace antflash {

struct URIFastParserMap {
public:

    enum EParseAction {
        URI_PARSE_CONTINUE = 0,
        URI_PARSE_CHECK = 1,
        URI_PARSE_BREAK = 2
    };

    URIFastParserMap() : action_map {URI_PARSE_CONTINUE,} {
        action_map['\0'] = URI_PARSE_BREAK;
        action_map[' '] = URI_PARSE_CHECK;
        action_map['#'] = URI_PARSE_BREAK;
        action_map['/'] = URI_PARSE_BREAK;
        action_map[':'] = URI_PARSE_CHECK;
        action_map['?'] = URI_PARSE_BREAK;
        action_map['@'] = URI_PARSE_CHECK;
    }

    EParseAction action_map[128]; //ascii
};


static URIFastParserMap s_fast_parser_map;

static const char *splitHostAndPort(
        const char *host_begin, const char *host_end, int *port) {
    *port = 0;
    int multiply = 1;
    for (const char *q = host_end - 1; q > host_begin; --q) {
        if (*q >= '0' && *q <= '9') {
            *port += (*q - '0') * multiply;
            multiply *= 10;
        } else if (*q == ':') {
            return q;
        } else {
            break;
        }
    }
    *port = -1;
    return host_end;
}

static bool isAllSpace(const char *p) {
    while (*p == ' ') {
        ++p;
    }
    return *p == '\0';
}

void URI::parseFromString(const char *url) {
    clear();
    const char *p = url;
    while (*p == ' ') {
        ++p;
    }
    const char *begin = p;

    bool need_schema = true;
    bool need_user_info = true;
    do {
        auto action = s_fast_parser_map.action_map[int(*p)];
        if (action == URIFastParserMap::URI_PARSE_CONTINUE) {
            continue;
        }
        if (action == URIFastParserMap::URI_PARSE_BREAK) {
            break;
        }

        if (*p == ':') {
            //always suppose that url has '\0' ending.
            if (p[1] == '/' && p[2] == '/' && need_schema) {
                need_schema = false;
                _schema.assign(begin, p - begin);
                p += 2;
                begin = p + 1;
            }
        } else if (*p == '@') {
            if (need_user_info) {
                need_user_info = false;
                _user_info.assign(begin, p - begin);
                begin = p + 1;
            }
        } else if (*p == ' ') {
            if (isAllSpace(p)) {
                _parse_ok = false;
                return;
            }
            break;
        }

    } while (++p);

    const char *host_end = splitHostAndPort(begin, p, &_port);
    _host.assign(begin, host_end - begin);
    if (*p == '/') {
        begin = p++;
        for (; *p && *p != '?' && *p != '#'; ++p) {
            if (*p == ' ') {
                if (isAllSpace(p)) {
                    _parse_ok = false;
                    return;
                }
                break;
            }
        }
        _path.assign(begin, p - begin);
    }
    if (*p == '?') {
        begin = ++p;
        for (; *p && *p != '#'; ++p) {
            if (*p == ' ') {
                if (!isAllSpace(p + 1)) {
                    _parse_ok = false;
                    return;
                }
                break;
            }
        }
        _query.assign(begin, p - begin);
    }
    if (*p == '#') {
        begin = ++p;
        for (; *p; ++p) {
            if (*p == ' ') {
                if (!isAllSpace(p + 1)) {
                    _parse_ok = false;
                    return;
                }
                break;
            }
        }
        _fragment.assign(begin, p - begin);
    }
}

}
