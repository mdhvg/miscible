#pragma once
#include "base/string.h"

enum SortType
{
    SortType_Path,
    SortType_Size,
    SortType_Filename,
    SortType_DateCreated,
    SortType_DateModified,
    SortType_EmbeddingDistance,
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

enum ByteUnit
{
    Byte,
    KiByte,
    MiByte,
    GiByte,
    TiByte,
    PiByte,
    ByteUnit_COUNT,
};

struct ByteSize
{
    F32 value;
    ByteUnit unit;
};

enum Month
{
    Jan,
    Feb,
    Mar,
    Apr,
    May,
    Jun,
    Jul,
    Aug,
    Sep,
    Oct,
    Nov,
    Dec,
    Month_COUNT
};

struct Date
{
    S32 date;
    Month month;
    S32 year;

    // NOTE: Later, maybe add time also?
    // U32 hour;
    // U32 minute;
    // U32 second;
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

enum Source
{
    Source_None,
    Source_Embedding,
    Source_FTS,
    Source_COUNT
};

struct ViewResultGroup
{
    S64 start_index;
    S64 count;
    Source source;
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
