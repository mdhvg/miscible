#pragma once
#include "db/fetch.h"
#include "inference/inference.h"

#define VIEW_FETCH_WINDOW_SIZE 400

enum SortType
{
    SortType_Directory,
    SortType_Filename,
    SortType_Size,
    SortType_DateAdded,
    SortType_DateCreated,
    SortType_DateModified,
    SortType_COUNT
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

enum SearchType
{
    SearchType_Text,
    SearchType_Embedding,
    SearchType_COUNT,
};

enum FilterType
{
    FilterType_SizeBetween,

    FilterType_Path,

    FilterType_DateAddedBetween,
    FilterType_DateCreatedBetween,
    FilterType_DateModifiedBetween,

    FilterType_EmbeddingDistanceBetween,

    FilterType_COUNT
};

struct ViewFilter
{
    FilterType type;
    B32 exclude;
    union {
        // NOTE: order from largest to smallest
        String val_str;
        Time val_date;
        ByteSize val_bytes;
        F32 val_float;
    };
};

enum QueryType
{
    QueryType_Default,
    QueryType_Embedding,
    QueryType_FTS,
    QueryType_COUNT
};

// NOTE: this struct keeps the inputs to the search
struct ViewQuery
{
    Arena *arena;
    String search_query;

    SortType sort_basis;
    B32 descending;

    ViewFilter *filters;
    Embedding embedding;

    QueryType query_type;

    // the limit (count) and offset for number of results returned
    // view_limit will always be <= 1.5 x (# images visible)
    S64 limit;
    S64 offset;

    U32 ticket;
};

struct ViewResult
{
    Arena *arena;
    S64 start, end;
    // array of data of current result window
    ImageMetadata *images;
    S64 count[QueryType_COUNT];

    U32 ticket;
};

struct ViewManager
{
    ViewQuery state;

    // total images in the result
    ViewResult main;
    ViewResult back;

    B32 busy;
};

MSCBL_API void view_init();
MSCBL_API void view_refresh();
MSCBL_API void view_fetch_map();
MSCBL_API void view_fetch_images(S64 start, S64 end);
