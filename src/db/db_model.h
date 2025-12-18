#pragma once

#include "base/base_core.h"
#include "base/string.h"

struct DBImageEntry
{
	U64 id;
	String path;
	String filename;
	U64 atlas_id;
	U32 atlas_idx;
	U64 size;
	U64 mtime;
	U64 ctime;
	U32 width;
	U32 height;
	U32 channels;
	B32 embedding;
};

struct DBAtlasEntry
{
	U64 id;
	String atlas_path;
	U32 image_count;
};