#pragma once
#include "os/os_inc.h"
#include "base/arena.h"

#define ThreadFunc(name) void name(Arena *arena, U64 id, struct TPData *args)
typedef ThreadFunc(thread_func);

struct Worker
{
    U64 id;
    Thread handle;
};

enum TPData_
{
    TPData_None,

    TPData_Any,

    TPData_U32,
    TPData_S64,
    TPData_U64,

    TPData_String,
    TPData_OSString,

    TPData_FileDesc,

    TPData_ArenaArr,

    TPData_COUNT
};

struct TPData
{
    TPData_ kind;
    union {
        void *val_any;

        U32 val_u32;
        S64 val_s64;
        U64 val_u64;

        FileHandle val_filedesc;

        String val_str;
        OSString val_os_str;

        ArenaArray val_arena_arr;
    };
};

struct AsyncTask
{
    thread_func *func;
    TPData args[5];

    volatile S64 *batch_size;
    Semaphore batch_complete;

#if DBG
    const char *queue_func;
#endif
};

void threadpool_init(U32 worker_count);
void threadpool_free();
U64 threadpool_worker_count();

enum TaskPriority
{
    TaskPriority_Realtime,
    TaskPriority_High,
    TaskPriority_Low,
};

#if DBG
#define threadpool_enqueue(p, t) _threadpool_enqueue(p, t, __func__)
MSCBL_API void _threadpool_enqueue(TaskPriority priority, AsyncTask task, const char *func_name);
#else
MSCBL_API void threadpool_enqueue(TaskPriority priority, AsyncTask task);
#endif
