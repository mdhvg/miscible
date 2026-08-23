// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include "db/fetch.h"
#include "inference/inference.h"

enum SortType
{
    SortType_Directory,
    SortType_Filename,
    SortType_Size,
    SortType_DateAdded,
    SortType_DateCreated,
    SortType_DateModified,
    SortType_COUNT,
};

enum GroupSubType
{
    Group_None = 0,

    Group_DateDay = 86400,
    Group_DateMonth = 2592000,
    Group_DateYear = 31536000,

    Group_Size1KB = KB(1),
    Group_Size10KB = KB(10),
    Group_Size100KB = KB(100),
    Group_Size1MB = MB(1),
    Group_Size10MB = MB(10),
    Group_Size100MB = MB(100),
    Group_Size1GB = GB(1),
};

enum FilterType
{
    FilterType_SizeBetween,

    FilterType_Path,

    FilterType_DateAddedBetween,
    FilterType_DateCreatedBetween,
    FilterType_DateModifiedBetween,

    FilterType_COUNT
};

enum SearchType
{
    SearchType_FTS,
    SearchType_Embedding,
    SearchType_COUNT
};

enum ViewQueryType
{
    ViewQuery_Default,
    ViewQuery_FTS,
    ViewQuery_Embedding,
};

MSCBL_API void view_fetch_map();
MSCBL_API void view_fetch_images(S64 start, S64 end);
MSCBL_API inline void view_fetch_new()
{
    view_fetch_map();
    view_fetch_images(0, 1);
}
