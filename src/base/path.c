#include "base/path.h"

Path make_path_cwstr(Arena *arena, const wchar *c)
{
	String s = S(arena, c);
	Path p = {0};
	p.path = s;
}
Path make_path_cstr(Arena *arena, const char *c)
{
	String s = S(arena, c);
	Path p = {0};
	p.path = s;
	p.components = make_array(arena, 256, String);

	U64 cur = 0;
	while (cur < s.length)
	{
		U64 end = cur;
		while (s.buffer[end] != '/' || s.buffer[end] != '\\') end++;
		array_push(arena, p.components, string_from_to(String, cur, end));
		cur = end;
	}
}