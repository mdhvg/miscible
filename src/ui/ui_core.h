#pragma once
#include "db/view.h"
#include "base/string.h"
#include "base/ringbuf.h"
#include "ui/pages/pages.h"

struct UIFilter
{
    B32 active;
    UIFilter *next;

    FilterType type;
    B32 exclude;
    union {
        // NOTE: order from largest to smallest
        StringBuilder val_str;
        Date val_date;
        ByteSize val_bytes;
        F32 val_float;
    };
};

struct UIViewQuery
{
    Arena *arena;

    StringBuilder search_query;
    SortType sort_basis;
    B32 descending;
    UIFilter *filters;
    U64 last;
};

struct UIState
{
    Arena *arena;
    Arena *page_arena;

    UIPage page;

    struct ImFont *ui_font;
    struct ImFont *icon_font;
    struct ImFont *title_font;

    UIViewQuery view_query;
};

MSCBL_API UIState ui_state;

MSCBL_API U64 put_dir(StringBuilder dir);
MSCBL_API void ui_add_filter();
MSCBL_API void ui_viewquery_clear();

void ui_init();
void ui_close();
void ui_update();
void ui_render();
B32 ui_needs_update();
