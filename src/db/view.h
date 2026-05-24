#pragma once
#include "base/string.h"

enum SortType
{
    SortType_Path,
    SortType_Size,
    SortType_Filename,
    SortType_DateCreated,
    SortType_DateModified,
    // SortType_EmbeddingDistance,
    SortType_COUNT
};

enum FilterType
{
    FilterType_SizeGreater,

    FilterType_Path,
    FilterType_Filename,

    FilterType_DateCreatedAfter,
    FilterType_DateModifiedAfter,

    FilterType_EmbeddingDistanceGreater,

    FilterType_COUNT
};

struct ViewFilter
{
    FilterType type;
    B32 exclude;
    union {
        // NOTE: order from largest to smallest
        String val_str;
        Date val_date;
        ByteSize val_bytes;
    };
};

struct ViewQuery
{
    Arena *arena;
    String search_query;

    SortType sort_basis;
    B32 descending;

    ViewFilter *filters;
};

enum QueryType
{
    QueryType_None,
    QueryType_Embedding,
    QueryType_FTS,
};

struct ViewResultGroup
{
    S64 start_index;
    S64 count;
    QueryType query_type;
};

struct ViewResult
{
    Arena *arena;
    S64 *image_ids;
    ViewResultGroup *groups;
};

struct ViewManager
{
    ViewQuery state;
    ViewResult main;
    ViewResult back;
    B32 busy;
};

MSCBL_API void view_init();
MSCBL_API void view_reload();
MSCBL_API ViewResult view_get_result();

MSCBL_API void view_clear_state();
MSCBL_API void view_set_state(struct UIViewQuery ui_query);
