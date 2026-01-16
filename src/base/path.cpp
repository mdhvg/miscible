#include "base/path.h"
#include "base/array.h"

Path make_path_cwstr(Arena *arena, const wchar *c)
{
    Path p = {0};
    string(arena, &p, c);
    //p.path = {0};
    //p.components = array_init(arena, 256, String);

    //U64 cur = 0;
    //while (cur < p.path.size)
    //{
    //	U64 end = cur;
    //	while (end < p.path.size && p.path.value[end] != L'/' && p.path.value[end] != L'\\') end++;
    //	array_push(arena, p.components, string_from_to(p.path, cur, end));
    //	cur = end + 1;
    //}
    return p;
}

Path make_path_cstr(Arena *arena, const char *c)
{
    Path p = {0};
    string(arena, &p, c);
    //p.path = {0};
    //p.components = array_init(arena, 256, String);

    //U64 cur = 0;
    //while (cur < p.path.size)
    //{
    //	U64 end = cur;
    //	while (end < p.path.size && p.path.value[end] != '/' && p.path.value[end] != '\\') end++;
    //	array_push(arena, p.components, string_from_to(p.path, cur, end));
    //	cur = end + 1;
    //}
    return p;
}

Path make_path_str(Arena *arena, String x)
{
    Path p = x;

    //U64 cur = 0;
    //while (cur < p.path.size)
    //{
    //	U64 end = cur;
    //	while (end < p.path.size && p.path.value[end] != '/' && p.path.value[end] != '\\') end++;
    //	array_push(arena, p.components, string_from_to(p.path, cur, end));
    //	cur = end + 1;
    //}
    return p;
}