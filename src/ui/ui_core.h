// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include "base/threadpool.h"
#include "db/fetch.h"
#include "db/view.h"
#include "base/string.h"
#include "gl/gl_core.h"
#include "ui/pages/menu/menu.h"
#include "ui/pages/pages.h"
#include "db/db_helpers.h"

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

struct UIViewQuery
{
    Arena *arena;
    UIFilter *filters;
    StringBuilder search_query;

    B32 descending;
    SortType sort_basis;
    GroupSubType sub_type;
    SearchType search_type;

    S64 visible_start, visible_end;

    DirKey selected_dir;

    U32 ticket;
    ViewQueryType query_type;
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

struct UIMap
{
    // What db returns
    union {
        S32 s32_header;
        Time time_header;
        String str_header;
        ByteSize size_header;
    };
    S64 item_count;

    // What I generate
    F32 y_offset;
    S64 global_group_offset;
};

struct UIMapResult
{
    Arena *arena;

    UIMap *map;

    ViewQueryType query_type;
    B32 y_offset_computed;
    U32 ticket;
};

struct UIMetadataResult
{
    Arena *arena;

    ImageMetadataArr images; // array of results in current visible window
};

struct UIResult
{
    UIMapResult main_map;
    UIMapResult back_map;

    UIMetadataResult main_list;
    UIMetadataResult back_list;
};

struct UIPreview
{
    U32 texture;
    S64 image_id;
    ImageMetadata metadata;
};

MSCBL_API UIState ui_state;
MSCBL_API UIResult ui_result;
MSCBL_API UIPreview ui_preview;

MSCBL_API U64 put_dir(StringBuilder dir);
MSCBL_API void ui_add_filter();
MSCBL_API void ui_viewquery_clear();
MSCBL_API void ui_push_message(Result message);

DBStmtCbk(push_image_metadata);

MSCBL_API inline void ui_preview_image(S64 image_id)
{
    ui_preview.image_id = image_id;

    sqlite3_stmt *stmt = db_prepare("SELECT * FROM Images WHERE id = ?;");
    sqlite3_bind_int64(stmt, 1, ui_preview.image_id);
    db_run_stmt(stmt, 1, push_image_metadata);

    AsyncTask task = {
        .func = gl_tex_id,
        .args = {
            {.kind = TPData_Any, .val_any = &ui_preview.texture},
            {.kind = TPData_S64, .val_s64 = image_id},
        },
    };
    threadpool_enqueue(TaskPriority_Realtime, task);
}

void ui_init();
void ui_close();
void ui_update();
void ui_render();
B32 ui_needs_update();
