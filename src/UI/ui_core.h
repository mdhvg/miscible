#pragma once

#include "Window/Window.h"
#include "UI/images.h"

enum View
{
	MENU,
	PREVIEW
};

struct UIState
{
	int active_image = 0;

	View view{MENU};

	bool menu_sidebar	= false;
	float sidebar_width = 0;
	double fps			= 0.0f;

	SortMode sorting{SORT_FILENAME};
	Arena *ui_arena;
};

struct AtlasTexture
{
	U64 db_id;
	U8 *data;
	U32 texture_id;
	B8 loaded;
};

struct UIPersist
{
	ArenaDoubleBuffer ui_arena;
	struct ImFont *ui_font;
	struct ImFont *icon_font;
	struct ImFont *title_font;

	DynamicArray(AtlasTexture, texture_data);

	U32 preview_texture;
	F32 sidebar_width;
	U8 sidebar_open;
};

global UIPersist ui_persist = {0};

void ui_init();
void ui_close();
void ui_update();
void ui_render();
void ui_count_atlas();
void ui_after_load();