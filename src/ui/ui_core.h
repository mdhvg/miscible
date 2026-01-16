#pragma once
#include "base/arena.h"
#include "base/array.h"
#include "base/tree.h"
#include "base/string.h"
#include "base/threadpool.h"
#include "window/window.h"

struct Atlas
{
    U64 db_id;
    U8 *data;
    U32 tex;
    B8 loaded;
};

struct Image
{
    U64 id;
    U64 atlas_id;
    U32 atlas_idx;
    U32 atlas_tex;
};

Tree_t(Image);
Tree_t(Atlas);

local_v S64 Image_cmp(Image *a, Image *b)
{
    return a->id - b->id;
}

local_v S64 Atlas_cmp(Atlas *a, Atlas *b)
{
    return a->db_id - b->db_id;
}

typedef void (*UIfn)(void);

enum UIPage
{
    UIPage_MENU = 0,
    UIPage_PREVIEW,
    UIPage_COUNT
};

struct PageData
{
    const char *fn_name;
    UIfn fn;
};

global_v PageData page_data[] = {
    {"ui_menu", NULL},
    {"ui_preview", NULL},
};

struct UIState
{
    Atlas_Tree atlas;
    Image_Tree images;

    struct ImFont *ui_font;
    struct ImFont *icon_font;
    struct ImFont *title_font;

    Image **display_order;
    U64 image_count;

    UIPage page;

    U64 active;
};

extern UIState ui_state;
extern StringBuilder strbuf;

global_v DynamicArray(U64, image_order) = {0};
global_v DynamicArray(Image, images)    = {0};

void ui_load_images();

void ui_init();
void ui_close();
void ui_update();
void ui_render();
void ui_count_atlas();
void ui_after_load();
void ui_image_tree();
THREAD_FUNC(ui_reload_order);
