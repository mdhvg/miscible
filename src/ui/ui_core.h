#pragma once
#include "db/view.h"
#include "base/string.h"
#include "ui/pages/pages.h"

struct UIFilter
{
    B32 active;
    U32 next;

    FilterType type;
    B32 exclude;
    union {
        // NOTE: order from largest to smallest
        StringBuilder val_str;
        Date val_date;
        ByteSize val_bytes;
    };
};

struct UIFilterList
{
    Arena *arena;

    StringBuilder search_query;
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

    UIFilterList filter_list;
};

MSCBL_API UIState ui_state;
// MSCBL_API sort_params current_sort;

MSCBL_API U64 put_dir(StringBuilder dir);
MSCBL_API void get_filename(U64 id, String *filename);

void ui_filterlist_init();
void ui_load_images();

void ui_init();
void ui_close();
void ui_update();
void ui_render();
void ui_after_load();
void ui_count_atlas();
