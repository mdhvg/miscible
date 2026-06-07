#pragma once
#include "base/threadpool.h"

MSCBL_API Arena *fetch_arena;

struct Image
{
    S64 id;
    S64 atlas_id;
    U32 atlas_idx;
    S64 parent_id, root_id;

    String path, filename;
    ByteSize size;
    Time mtime, ctime;
    S32 width, height, channels;
};

struct Atlas
{
    S64 id;
    U32 tex;
};

typedef U64 DirKey;

struct DirTree
{
    S64 id;
    U64 level;
    String name;
    DirKey next_idx;
    DirKey child_idx;
};

#define dense_update(arr, idx, v)      \
    do                                 \
    {                                  \
        while (va_getsize(arr) <= idx) \
            va_push(arr, {0});         \
        arr[idx] = v;                  \
    } while (0)

MSCBL_API Image *images;
MSCBL_API Atlas *atlases;
MSCBL_API DirTree *dir_tree;

OSString *db_fetch_pending();
MSCBL_API ThreadFunc(db_fetchall);
