#pragma once
#include "base/string.h"
#include "base/ringbuf.h"
#include "os/os_inc.h"

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
    SortType sort_basis;
    B32 descending;

    ViewFilter *filters;

    StringBuilder search_query;
};

struct ViewRequest
{
    Arena *arena;
    ViewQuery query;

    F32 *search_embedding;
    U32 embedding_dim;

    U32 ticket;
};

struct ViewResultGroup
{
    String title;
    S64 start_index;
    U64 count;
};

struct ViewResult
{
    Arena *arena;
    S64 *image_ids;
    ViewResultGroup *groups;
};

struct ViewManager
{
    Mutex queue_mutex;
    RingBuffer(ViewRequest, request_queue, 16);

    ViewRequest state;
    U32 ticket_counter;

    ViewResult main;
    ViewResult back;
};

MSCBL_API S64 *view_order;
MSCBL_API void view_init();
MSCBL_API void view_fetch();
MSCBL_API void view_reload();

MSCBL_API void view_update_search(String query);
MSCBL_API void view_push_filters(struct UIFilter *filters);
MSCBL_API ViewFilter **view_get_filters();

MSCBL_API SortType view_get_order();
MSCBL_API void view_set_order(SortType sort);
// MSCBL_API void view_set_order(ViewOrder order);
