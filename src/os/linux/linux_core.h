// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include <pthread.h>
#include <sys/types.h>
#include <semaphore.h>

#include "base/string.h"

#define OS_THREAD_ROUTINE(name) void *name(void *data)

#define OSSlash  "/"
#define LibExt   ".so"

typedef char OSChar;
typedef Str8 OSString;

typedef int FileHandle;
typedef void *LibHandle;
typedef void *LibAddress;

typedef pid_t Process;
typedef pthread_t Thread;
typedef sem_t Semaphore;
typedef pthread_mutex_t Mutex;
typedef void *(ThreadRoutine_t(void *));

struct OSInfo
{
    Time start_time;
    U64 start_ticks;
    U64 ticks_per_sec;

    U64 nproc;
    U32 proc_id;
    Process process;

    U64 page_size;
    Arena *arena;
};

const char *os_select_dir(const char *title, const char *default_path);
