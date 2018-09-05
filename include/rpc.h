//
// Created by zhenggu.xwt on 18/4/18.
//

#ifndef RPC_INCLUDE_RPC_H
#define RPC_INCLUDE_RPC_H

#include "channel/channel.h"
#include "session/session.h"
#include "protocol/http/http_request.h"
#include "protocol/http/http_response.h"
#include "protocol/bolt/bolt_request.h"
#include "protocol/bolt/bolt_response.h"

namespace antflash {

/**
 * Global init for ant feature rpc client, not thread compatible,
 * you should call this method before using rpc client.
 * Init action: scheduler threads, timer threads and socket manager
 * thread.
 * @return true if init success, else false
 */
bool globalInit();

/**
 * Global destroy for ant feature rpc client, not thread compatible,
 * you should call this method before process end, or some exception may
 * be abort.
 */
void globalDestroy();

/**
 * Set Specific rpc log handler, if not set, rpc will log out information to stdout
 * @param handler
 * @return
 */
LogHandler* setLogHandler(LogHandler* handler);

/**
 * Set rpc log level, info level default.
 * @param level
 * @return
 */
void setLogLevel(LogLevel level);

}

#endif //RPC_INCLUDE_RPC_H
