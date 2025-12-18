#include "Images/dir_walker.h"
#include "base/base_core.h"
#include "base/string.h"
#include "base/stack.h"
#include "base/path.h"
#include "db/db_helpers.h"

#if OS_WINDOWS
#include <windows.h>
#elif OS_LINUX
#endif
#include <stdio.h>

#include <stdlib.h>

#define MAX_DEPTH		 15
#define IMAGE_BATCH_SIZE 1000
#define PUSH_LIMIT		 10
#define SCAN_ARENA_SIZE	 MB(100)
#define FILE_ARENA_SIZE	 MB(1)

local Arena *scan_arena	   = NULL;
local Arena *scratch_arena = NULL;
local Arena *file_arena	   = NULL;

struct PathNode
{
	Path path;
	U64 depth;
	U64 scanned;
};

stack_def(path_stack, PathNode, 1000);
stack_def(cur_stack, PathNode, 800);
stack_def(statements, String, IMAGE_BATCH_SIZE);

#if OS_WINDOWS
void walk_directories(void *data)
{
	if (!scan_arena) scan_arena = arena_alloc(SCAN_ARENA_SIZE);
	arena_clear(scan_arena);
	if (!scratch_arena) scratch_arena = arena_alloc(KB(500));
	arena_clear(scratch_arena);
	if (!file_arena) file_arena = arena_alloc(FILE_ARENA_SIZE);
	arena_clear(file_arena);

	PathNode root = {S(ROOT_DIR "/test_imgs")};
	stack_push(path_stack, root);

	WIN32_FIND_DATA fdFile;
	HANDLE hFind = NULL;

	U64 scan_count = 0;
	while (!stack_empty(path_stack))
	{
		PathNode *cur = stack_front(path_stack);

		Path expression = string_format(scratch_arena, "%.*s/*.*", cur->path.size, cur->path.v);
		scan_count		= 0;
		for (hFind = FindFirstFile((const char *)expression.v, &fdFile); FindNextFile(hFind, &fdFile) && scan_count < cur->scanned + PUSH_LIMIT;)
		{
			if (hFind == INVALID_HANDLE_VALUE)
			{
				stack_pop(path_stack);
				break;
			}
			if (scan_count < cur->scanned || match_front(fdFile.cFileName, ".") || match_front(fdFile.cFileName, ".."))
			{
				scan_count++;
				continue;
			}
			if (fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && cur->depth + 1 < MAX_DEPTH)
			{
				PathNode new_dir = {string_format(scan_arena, "%.*s/%s", cur->path.size, (char *)cur->path.v, &fdFile.cFileName), cur->depth + 1};
				stack_push(cur_stack, new_dir);
			}
			else
			// TODO: Find attribute combinatin to locate all image files safely (without falling for symlinks, system files, etc)
			// if (fdFile.dwFileAttributes & FILE_ATTRIBUTE_NORMAL)
			{
				// TODO: Make conversion to UNIX time
				FILETIME write_time = fdFile.ftLastWriteTime;
				FILETIME make_time	= fdFile.ftCreationTime;
				U64 mtime = 0, ctime = 0, size = 0;
				mtime = write_time.dwHighDateTime;
				mtime <<= 32;
				mtime |= write_time.dwLowDateTime;
				ctime = make_time.dwHighDateTime;
				ctime <<= 32;
				ctime |= make_time.dwLowDateTime;
				size = fdFile.nFileSizeHigh;
				size <<= 32;
				size |= fdFile.nFileSizeLow;

				// TODO: Check filetype using magic bytes
				if (match_end(fdFile.cFileName, ".jpg") || match_end(fdFile.cFileName, ".png"))
					stack_push(statements, string_format(file_arena, R"(INSERT INTO Images(path, filename, size, mtime, ctime) VALUES('%.*s/%s', '%s', %zu, %zu, %zu) ON CONFLICT(path) DO NOTHING;)",
														 cur->path.size, (char *)cur->path.v, &fdFile.cFileName, &fdFile.cFileName, size, mtime, ctime));
				if (stack_full(statements))
				{
					db_run("BEGIN TRANSACTION;");
					while (!stack_empty(statements))
					{
						db_run(stack_pop(statements));
					}
					db_run("COMMIT;");
					arena_clear(file_arena);
				}
			}
			scan_count++;
		}
		if (FindNextFile(hFind, &fdFile))
		{
			cur->scanned = scan_count;
		}
		else
		{
			stack_pop(path_stack);
		}
		arena_clear(scratch_arena);
		while (!stack_empty(cur_stack)) { stack_push(path_stack, stack_pop(cur_stack)); }

		FindClose(hFind);
	}
	db_run("BEGIN TRANSACTION;");
	while (!stack_empty(statements))
	{
		db_run(stack_pop(statements));
	}
	db_run("COMMIT;");
}
#elif OS_LINUX
#endif