#pragma once
#include <sys/random.h>

#include "base/base_core.h"
#include "base/threadpool.h"
#include "base/log.h"

typedef void *(OS_THREAD_ROUTINE_T(void *));
#define OS_THREAD_ROUTINE(name) void *name(void *data)
#define OSAssert(x)                                                                                      \
    {                                                                                                    \
        if (!(x))                                                                                        \
        {                                                                                                \
            printf("[" ASCII_RED "%s" ASCII_RESET "] Error Code: 0x%X (%lu)\n", __func__, errno, errno); \
            Assert(0);                                                                                   \
        }                                                                                                \
        else                                                                                             \
        {                                                                                                \
            ((void)0);                                                                                   \
        }                                                                                                \
    }

struct OSInfo
{
    OS_COMMON;
};

Thread os_thread_launch(OS_THREAD_ROUTINE_T fn, Worker *worker);
