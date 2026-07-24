#pragma once
#include "db/fetch.h"
#include "db/view.h"
#include "base/string.h"
#include "ui/pages/menu/menu.h"
#include "ui/pages/pages.h"

struct UIFilter
{
    B32 active;
    UIFilter *next;

    FilterType type;
    union {
        // NOTE: order from largest to smallest
        struct
        {
            Time from;
            Time to;
            B32 from_enable;
            B32 to_enable;
        } val_time;
        struct
        {
            StringBuilder val;
            B32 exclude;
        } val_str;
        struct
        {
            ByteSize from;
            ByteSize to;
            B32 from_enable;
            B32 to_enable;
        } val_byte;
        struct
        {
            F32 from;
            F32 to;
            B32 from_enable;
            B32 to_enable;
        } val_float;
    };
};

struct UIImageResult
{
    Arena *arena;
    U64 arena_stop;
    Embedding embedding;

    // What db returns
    union {
        TimeArr time_headers;
        StringArr str_headers;
        ByteSizeArr size_headers;
    };
    S64 *item_counts;

    S64 start, end;
    ImageMetadataArr *images; // array of results in current visible window

    // What I generate
    F32 *y_offset;
    S64 *global_group_offset;
};

struct UIViewQuery
{
    Arena *arena;
    UIFilter *filters;
    StringBuilder search_query;

    B32 descending;
    SortType sort_basis;
    QueryType query_type;
    GroupSubType sub_type;

    DirKey selected_dir;
};

struct UIPreview
{
    U32 texture;
    S64 image_id;
};

struct UIState
{
    Arena *arena;

    UIPage page;

    struct ImFont *ui_font;
    struct ImFont *icon_font;
    struct ImFont *title_font;
    struct ImFont *display_font;

    U32 icon_texture;

    UIViewQuery view_query;
};

struct UISearchQuery
{
    SearchType type;
    UIImageResult main;
    UIImageResult back;
};

MSCBL_API UIState ui_state;
MSCBL_API UIPreview ui_preview;
MSCBL_API UISearchQuery ui_search;

MSCBL_API U64 put_dir(StringBuilder dir);
MSCBL_API void ui_add_filter();
MSCBL_API void ui_viewquery_clear();
MSCBL_API void ui_push_message(Result message);

void ui_init();
void ui_close();
void ui_update();
void ui_render();
B32 ui_needs_update();
