#pragma once

#include "base/threadpool.h"
#include "base/array.h"
#include "base/path.h"

struct ImageEntry
{
	U64 id;
	Path path;
};

struct Thumbnail
{
	U8 *data;
	S32 width;
	S32 height;
	S32 channels;
	S32 resize_width;
	S32 resize_height;
};

struct AtlasDrawData
{
	StaticArray(ImageEntry, image_entries);
	U64 draw_count;
	U8 cur_draw_count;
	U8 *atlas_data;
};

global AtlasDrawData atlas_draw_data = {0};

// TODO: Partial array fit
// global StaticArray(DBAtlasEntry, partial_atlas);

THREAD_FUNC(load_thumbnails);

void atlas_render_prepare();
void atlas_draw_one();
void atlas_after_draw();
void atlas_load_batch();