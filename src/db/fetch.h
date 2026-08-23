// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include "base/threadpool.h"

struct ImageMetadata
{
    S64 id;
    String path, filename;

    S64 atlas_id;
    U32 atlas_idx;

    ByteSize size;

    Time ctime, mtime;
    Time atime;

    S32 width, height, channels;

    S64 root_dir, parent_dir;
};
typedef ImageMetadata *ImageMetadataArr;

struct AtlasMap
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

#define dense_update(arr, idx, v)       \
    do                                  \
    {                                   \
        while (arr_getsize(arr) <= idx) \
            va_push(arr, {0});          \
        arr[idx] = v;                   \
    } while (0)

MSCBL_API AtlasMap *fetch_atlases;
MSCBL_API DirTree *fetch_directories;

OSString *db_fetch_pending();
MSCBL_API ThreadFunc(db_fetchall);
