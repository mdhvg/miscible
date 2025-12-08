#pragma once

#include "base/core.h"
#include "base/string.h"
#include "base/arena.h"

typedef String Path;

Path make_path_cwstr(Arena *arena, const wchar *c);
Path make_path_cstr(Arena *arena, const char *c);
Path make_path_str(Arena *arena, String x);

#if defined(__cplusplus)
inline Path make_path(Arena *arena, const char *x)
{
	return make_path_cstr(arena, x);
}
inline Path make_path(Arena *arena, const wchar *x)
{
	return make_path_cwstr(arena, x);
}
inline Path make_path(Arena *arena, String x)
{
	return make_path_str(arena, x);
}
// inline void make_path(Arena *arena, Path* p, WString x)
// {
// 	make_path_wstr(arena, p, x);
// }
#else // __cplusplus
#define make_path(arena, p, x)    \
	_Generic((x),                 \
		char *: make_path_cstr,   \
		wchar *: make_path_cwstr, \
		String: make_path_str,    \
		WString: make_path_wstr,  \
		default: make_path_cstr)(arena, p, x)
#endif // __cplusplus