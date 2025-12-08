#include "atlas_render.h"
#include "base/string.h"
#include <cstddef>
#include <fileapi.h>
#include <handleapi.h>
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

#include "base/core.h"
#include "base/path.h"
#include "atlas/atlas_render.h"
#include "Images/dir_walker.h"
#include "base/threadpool.h"
#include "base/arena.h"
#include "os/os_inc.h"
#include "db/db_helpers.h"
#include "db/db_model.h"

struct DrawParams
{
	const char *image_path;
	U8 *atlas_data;
	U64 atlas_id;
	U64 image_id;
	S32 index;
	std::atomic<U32> *drawn;
};

struct ImageEntry
{
	U64 id;
	Path path;
};

ImageEntry *entries = NULL;
U64 entry_ptr = 0;

DBAtlasEntry *partial_atlas = NULL;
U64 partial_ptr = 0;

struct Thumbnail
{
	U8 *data;
	S32 width;
	S32 height;
	S32 channels;
};

Thumbnail thumbnail_data[ATLAS_CAPACITY] = {0};
local Arena *atlas_arena = NULL;

DB_CALLBACK(get_count)
{
	U64 *count = (U64 *)data;
	*count = strtoull(argv[0], NULL, 10);
	return 0;
}

DB_CALLBACK(get_entries)
{
	entries[entry_ptr++] = {
		strtoull(argv[0], NULL, 0),
		s_cpy(scratch_arena, argv[1])};
	return 0;
}

DB_CALLBACK(get_partial)
{
	partial_atlas[partial_ptr++] = {
		strtoull(argv[0], NULL, 0),
		s_cpy(scratch_arena, argv[1]),
		strtoul(argv[2], NULL, 0)};
	return 0;
}

DB_CALLBACK(new_atlas)
{
	U64 *id = (U64 *)data;
	*id = strtoull(argv[0], NULL, 0);
	return 0;
}

void draw_thumbnails(U8 *atlas_data, U32 start, U32 count)
{
	U32 ptr = 0;
	Thumbnail t = {0};
	while (ptr < count)
	{
		t = thumbnail_data[ptr];
		U32 smaller_side = MIN(t.width, t.height);

		U32 x_off = (t.width - smaller_side) / 2;
		U32 y_off = (t.height - smaller_side) / 2;

		U32 idx = start + ptr;
		U8 *src = t.data + (y_off * t.width + x_off) * 3;
		U8 *dst = atlas_data + (idx / THUMB_PER_SIDE * ATLAS_SIZE * 3 * THUMB_SIZE) +
				  (idx % THUMB_PER_SIDE * 3 * THUMB_SIZE);

		for (U32 i = 0; i < THUMB_SIZE; i++)
		{
			memcpy(dst, src, THUMB_SIZE * 3);
			src += t.width * 3;
			dst += ATLAS_SIZE * 3;
		}
		ptr++;
	}
}

// U32 load_thumbnails(U32 start, U32 count)
// {
// 	U8 ptr = 0;
// 	S32 w, h, c;
// 	while (ptr < count)
// 	{
// 		U8 *data = stbi_load(str_to_cstr(entries[start + ptr].path), &w, &h, &c, 3);
// 		S32 resize_height, resize_width;
// 		if (w < h)
// 		{
// 			resize_width = THUMB_SIZE;
// 			resize_height = THUMB_SIZE / ((float)w / (float)h);
// 		}
// 		else
// 		{
// 			resize_height = THUMB_SIZE;
// 			resize_width = THUMB_SIZE * ((float)w / (float)h);
// 		}

// 		U8 *resize_data = push_size(atlas_arena, (3 * resize_height * resize_width), U8);
// 		stbir_resize_uint8_linear(data, w, h, 0, resize_data, resize_width, resize_height, 0, STBIR_RGB);
// 		stbi_image_free(data);
// 		thumbnail_data[ptr] = {resize_data, resize_width, resize_height, c};
// 		ptr++;
// 	}

// 	return count;
// }

THREAD_FUNC(load_thumbnails)
{
	U64 start = *(U64 *)data;
	S32 w, h, c;

	U8 *img_data = stbi_load(str_to_cstr(entries[start + n].path), &w, &h, &c, 3);
	S32 resize_height, resize_width;
	if (w < h)
	{
		resize_width = THUMB_SIZE;
		resize_height = THUMB_SIZE / ((float)w / (float)h);
	}
	else
	{
		resize_height = THUMB_SIZE;
		resize_width = THUMB_SIZE * ((float)w / (float)h);
	}

	U8 *resize_data = push_size(atlas_arena, (3 * resize_height * resize_width), U8);
	stbir_resize_uint8_linear(img_data, w, h, 0, resize_data, resize_width, resize_height, 0, STBIR_RGB);
	stbi_image_free(img_data);
	thumbnail_data[n] = {resize_data, resize_width, resize_height, c};
}

void update_db(DBAtlasEntry a, U32 atlas_start, U32 entry_start, U32 count)
{
	U32 ptr = 0;
	Thumbnail t = {0};
	String statement;
	while (ptr < count)
	{
		t = thumbnail_data[ptr];
		U32 atlas_idx = atlas_start + ptr;
		U32 entry_idx = entry_start + ptr;
		statement = string_format(scratch_arena, R"(UPDATE Images SET atlas_id = %zu, atlas_idx = %d, width = %d, height = %d, channels = %d WHERE id = %zu;)", a.id, atlas_idx, t.width, t.height, t.channels, entries[entry_idx].id);
		db_run(statement);
		ptr++;
	}
	statement = string_format(scratch_arena, "UPDATE Atlas SET image_count = image_count + %d WHERE id = %zu;", count, a.id);
	db_run(statement);
}

void atlas_init(void *)
{
	if (!atlas_arena) atlas_arena = arena_alloc(MB(100));
	arena_clear(atlas_arena);

	if (!scratch_arena) scratch_arena = arena_alloc(MB(1));
	arena_clear(scratch_arena);

	entry_ptr = 0;
	partial_ptr = 0;

	U64 image_count = 0;
	String statement = S(R"(SELECT COUNT(id) FROM Images WHERE atlas_id IS NULL;)");
	db_run(statement, get_count, &image_count);

	entries = push_array(scratch_arena, image_count, ImageEntry);
	statement = S(R"(SELECT id, path FROM Images WHERE atlas_id IS NULL;)");
	db_run(statement, get_entries);

	U64 partial_count = 0;
	statement = S("SELECT COUNT(id) FROM Atlas WHERE image_count < " Stringify(ATLAS_CAPACITY) ";");
	db_run(statement, get_count, &partial_count);

	partial_atlas = push_array(scratch_arena, partial_count, DBAtlasEntry);
	statement = S("SELECT id, atlas_path, image_count FROM Atlas WHERE image_count < " Stringify(ATLAS_CAPACITY) ";");
	db_run(statement, get_partial);

	// U32 cur_image = 0,
	// 	cur_atlas = 0,
	// 	atlas_cap = 0;
	// U8 *atlas_data = NULL;
	// DBAtlasEntry atlas = {0};
	// Temp t = temp_begin(scratch_arena);

	// while (cur_image < image_count)
	// {
	// 	t = temp_begin(scratch_arena);
	// 	arena_clear(atlas_arena);

	// 	db_run("BEGIN TRANSACTION;");
	// 	if (cur_atlas < partial_count)
	// 	{
	// 		atlas_cap = ATLAS_CAPACITY - partial_atlas[cur_atlas].image_count;
	// 		S32 w, h;
	// 		atlas_data = stbi_load((const char *)partial_atlas[cur_atlas].atlas_path.value, &w, &h, NULL, 3);
	// 		atlas = partial_atlas[cur_atlas];
	// 	}
	// 	else
	// 	{
	// 		atlas_cap = ATLAS_CAPACITY;
	// 		atlas_data = push_array(atlas_arena, (ATLAS_SIZE * ATLAS_SIZE * 3), U8);

	// 		char *guid = push_array(scratch_arena, sizeof(Guid) * 2, char);
	// 		bytes_as_hex_lower(os_make_guid().v, 0, sizeof(Guid), guid);
	// 		atlas.atlas_path = string_format(scratch_arena, ROOT_DIR "/.atlas/%.*s.tga", sizeof(Guid) * 2, guid);
	// 		atlas.image_count = 0;
	// 		String statement = string_format(scratch_arena, "INSERT INTO Atlas(atlas_path, image_count) VALUES('%.*s', 0) RETURNING id;", atlas.atlas_path.size, atlas.atlas_path.value);
	// 		db_run(statement, new_atlas, &atlas.id);
	// 	}

	// 	U32 loaded = load_thumbnails(cur_image, MIN(image_count - cur_image, atlas_cap));
	// 	draw_thumbnails(atlas_data, ATLAS_CAPACITY - atlas_cap, loaded);
	// 	update_db(atlas, ATLAS_CAPACITY - atlas_cap, cur_image, loaded);
	// 	db_run("COMMIT;");
	// 	stbi_write_tga(str_to_cstr(atlas.atlas_path), ATLAS_SIZE, ATLAS_SIZE, 3, atlas_data);
	// 	printf("Written atlas: %.*s\n", atlas.atlas_path.size, atlas.atlas_path.value);
	// 	cur_image += loaded;

	// 	temp_end(t);
	// }
}
