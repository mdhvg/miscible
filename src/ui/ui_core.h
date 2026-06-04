#pragma once
#include "db/view.h"
#include "base/string.h"
#include "gl/gl_core.h"
#include "os/win32/win32_core.h"
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
    U64 semantic_search_limit;
};

struct UIPreview
{
    U32 texture;
    S64 image_id;
    gl_args_path render_args;
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
MSCBL_API UIPreview ui_preview;

MSCBL_API U64 put_dir(StringBuilder dir);
MSCBL_API void ui_add_filter();
MSCBL_API void ui_viewquery_clear();
MSCBL_API void ui_push_message(Result message);

void ui_init();
void ui_close();
void ui_update();
void ui_render();
B32 ui_needs_update();
