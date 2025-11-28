#include "base/core.h"
#include "base/string.h"
#include "base/arena.h"

struct Path
{
	StringArray components;
	String path;
};

#define make_path(arena, x)     \
	_Generic((x),                 \
			char *: make_path_cstr,   \
			wchar *: make_path_cwstr, \
			String: make_path_str,    \
			WString: make_path_wstr,  \
			default: make_path_cstr)(arena, x)

Path make_path_cwstr(Arena *arena, const wchar *c);
Path make_path_cstr(Arena *arena, const char *c);