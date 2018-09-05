// Copyright (c) 2018 Ant Financial, Inc. All Rights Reserved
// Created by zhenggu.xwt on 18/4/10.
//

#ifndef RPC_COMMON_MARCO_H
#define RPC_COMMON_MARCO_H

#undef LIKELY
#undef UNLIKELY
#if defined(OS_LINUX)
#define LIKELY(expr) (__builtin_expect((bool)(expr), true))
#define UNLIKELY(expr) (__builtin_expect((bool)(expr), false))
#else
#define LIKELY(expr) (expr)
#define UNLIKELY(expr) (expr)
#endif

#endif //RPC_COMMON_MARCO_H
