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
    FilterType_Path,
    FilterType_Filename,

    FilterType_SizeGreater,

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
        String val_str;
        U32 val_int;
        F32 val_float;
    };
    ViewFilter *next;
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
MSCBL_API void view_push_filters(ViewFilter* filters);

MSCBL_API SortType view_get_order();
MSCBL_API void view_set_order(SortType sort);
// MSCBL_API void view_set_order(ViewOrder order);
