#include "base/array.h"
#include "gl/gl_core.h"
#include "ui/ui_core.h"
#include "base/task_runner.h"
#include "os/win32/os_core_win32.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "base/base_core.h"
#include "atlas/atlas_render.h"
#include "Images/dir_walker.h"
#include "base/threadpool.h"
#include "base/arena.h"
#include "os/os_inc.h"
#include "db/db_helpers.h"
#include "db/db_model.h"
#include "atlas_render.h"
#include "base/string.h"

local Arena *atlas_arena = NULL;
local Thumbnail thumbnail_data[ATLAS_CAPACITY];
local Temp t = {0};

DB_CALLBACK(get_entries)
{
	Assert(atlas_draw_data.image_entries.size + 1 <= atlas_draw_data.image_entries.max);
	atlas_draw_data.image_entries.v[atlas_draw_data.image_entries.size++] = {strtoull(argv[0], NULL, 0), s_cpy(scratch_arena, argv[1])};
	return 0;
}

// DB_CALLBACK(get_partial)
// {
// 	Assert(partial_atlas.size + 1 <= partial_atlas.max);
// 	partial_atlas.v[partial_atlas.size++] = {strtoull(argv[0], NULL, 0), s_cpy(scratch_arena, argv[1]), strtoul(argv[2], NULL, 0)};
// 	return 0;
// }

THREAD_FUNC(draw_thumbnails) // (U8 *atlas_data, U32 start, U32 count)
{
	U32 smaller_side = MIN(thumbnail_data[n].resize_width, thumbnail_data[n].resize_height);

	U32 x_off = (thumbnail_data[n].resize_width - smaller_side) / 2;
	U32 y_off = (thumbnail_data[n].resize_height - smaller_side) / 2;

	U8 *src = thumbnail_data[n].data + (y_off * thumbnail_data[n].resize_width + x_off) * 3;
	U8 *dst = atlas_draw_data.atlas_data + (n / THUMB_PER_SIDE * ATLAS_SIZE * 3 * THUMB_SIZE) +
			  (n % THUMB_PER_SIDE * 3 * THUMB_SIZE);

	for (U32 i = 0; i < THUMB_SIZE; i++)
	{
		memcpy(dst, src, THUMB_SIZE * 3);
		src += thumbnail_data[n].resize_width * 3;
		dst += ATLAS_SIZE * 3;
	}
}

THREAD_FUNC(load_thumbnails)
{
	S32 w, h, c;
	S32 resize_height, resize_width;
	U8 *img_data = stbi_load(str_to_cstr(atlas_draw_data.image_entries.v[n + atlas_draw_data.draw_count].path), &w, &h, &c, 3);
	if (w < h)
	{
		resize_width  = THUMB_SIZE;
		resize_height = THUMB_SIZE / ((float)w / (float)h);
	}
	else
	{
		resize_height = THUMB_SIZE;
		resize_width  = THUMB_SIZE * ((float)w / (float)h);
	}

	U8 *resize_data = push_size(atlas_arena, (3 * resize_height * resize_width), U8);
	stbir_resize_uint8_linear(img_data, w, h, 0, resize_data, resize_width, resize_height, 0, STBIR_RGB);
	stbi_image_free(img_data);
	thumbnail_data[n] = {resize_data, w, h, c, resize_width, resize_height};
}

void atlas_render_prepare()
{
	if (!atlas_arena) atlas_arena = arena_alloc(MB(100));
	arena_clear(atlas_arena);

	if (!scratch_arena) scratch_arena = arena_alloc(MB(1));
	arena_clear(scratch_arena);

	U64 image_count	 = 0;
	String statement = S(R"(SELECT COUNT(id) FROM Images WHERE atlas_id IS NULL;)");
	db_run(statement, get_count, &image_count);

	// U64 partial_count = 0;
	// statement = S("SELECT COUNT(id) FROM Atlas WHERE image_count < " Stringify(ATLAS_CAPACITY) ";");
	// db_run(statement, get_count, &partial_count);

	atlas_draw_data.image_entries = {push_array(scratch_arena, image_count, ImageEntry), 0, image_count};
	// partial_atlas = {push_array(scratch_arena, partial_count, DBAtlasEntry), 0, partial_count};

	statement = S(R"(SELECT id, path FROM Images WHERE atlas_id IS NULL;)");
	db_run(statement, get_entries);
	// statement = S("SELECT id, atlas_path, image_count FROM Atlas WHERE image_count < " Stringify(ATLAS_CAPACITY) ";");
	// db_run(statement, get_partial);
}

void atlas_load_batch()
{
	t							   = temp_begin(scratch_arena);
	atlas_draw_data.cur_draw_count = MIN(atlas_draw_data.image_entries.size - atlas_draw_data.draw_count, ATLAS_CAPACITY);
	parallel_for(os_info.pool, atlas_draw_data.cur_draw_count, load_thumbnails, NULL, NULL);
}

void atlas_draw_one()
{
	if (atlas_draw_data.draw_count < atlas_draw_data.image_entries.size)
	{
		atlas_draw_data.atlas_data = push_size(atlas_arena, (ATLAS_SIZE * ATLAS_SIZE * 3), U8);
		parallel_for(os_info.pool, atlas_draw_data.cur_draw_count, draw_thumbnails, NULL, NULL);
	}
}

void atlas_after_draw()
{
	if (!atlas_draw_data.cur_draw_count) return;
	char *guid = push_array(scratch_arena, sizeof(Guid) * 2, char);
	bytes_as_hex_lower(os_make_guid().v, 0, sizeof(Guid), guid);
	String path = string_format(scratch_arena, ROOT_DIR "/.atlas/%.*s.tga", sizeof(Guid) * 2, guid);

	db_run("BEGIN TRANSACTION;");
	U64 atlas_id;
	String statement = string_format(scratch_arena, "INSERT INTO Atlas(atlas_path, image_count) VALUES('%.*s', %zu) RETURNING id;", path.size, path.v, atlas_draw_data.cur_draw_count);
	db_run(statement, get_count, &atlas_id);

	U64 i = 0;
	while (i < atlas_draw_data.cur_draw_count)
	{
		S32 width	 = thumbnail_data[i + atlas_draw_data.draw_count].width;
		S32 height	 = thumbnail_data[i + atlas_draw_data.draw_count].height;
		S32 channels = thumbnail_data[i + atlas_draw_data.draw_count].channels;
		U64 id		 = atlas_draw_data.image_entries.v[i + atlas_draw_data.draw_count].id;
		statement	 = string_format(
			   scratch_arena,
			   R"(UPDATE Images SET atlas_id = %zu, atlas_idx = %d, width = %d, height = %d, channels = %d WHERE id = %zu;)",
			   atlas_id, i, width, height, channels, id);
		db_run(statement);
		i++;
	}

	db_run("COMMIT;");

	stbi_write_tga(str_to_cstr(path), ATLAS_SIZE, ATLAS_SIZE, 3, atlas_draw_data.atlas_data);
	printf("Written atlas: %.*s\n", path.size, str_to_cstr(path));

	U32 tex_id;
	gl_make_texture(&tex_id, atlas_draw_data.atlas_data, ATLAS_SIZE, ATLAS_SIZE, 3);
	AtlasTexture x = {atlas_id, atlas_draw_data.atlas_data, tex_id, 1};
	dyn_array_push(pics.persistent_arena, ui_persist.texture_data, x);

	atlas_draw_data.draw_count += atlas_draw_data.cur_draw_count;
	atlas_draw_data.atlas_data = NULL;
	arena_clear(atlas_arena);
	temp_end(t);
	if (atlas_draw_data.draw_count < atlas_draw_data.image_entries.size)
	{
		push_task(TASK_LOAD_THUMBNAILS);
		push_task(TASK_DRAW_THUMBNAILS);
		push_task(TASK_LOAD_ATLAS);
	}
}