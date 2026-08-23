// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include <sys/random.h>

#include "base/base_core.h"
#include "base/threadpool.h"
#include "base/log.h"

#define W(x)       #x
#define LibHandle  void *
#define LibAddress void *
#define LibExt     ".so"

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
#define OSChar    char
#define Semaphore sem_t
#define Thread    pthread_t

struct OSInfo
{
    OS_COMMON;
};

const char *os_select_dir(const char *title, const char *default_path);
Thread os_thread_launch(OS_THREAD_ROUTINE_T fn, Worker *worker);
