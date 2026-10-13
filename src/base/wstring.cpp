// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "base/log.h"
#include "base/arena.h"
#include "base/wstring.h"

// Str16 make_string_cwstr(const wchar *s)
// {
// #if defined WCHAR_UTF16
//     if (!s)
//         return {(U16 *)L"", 0};
//     U64 len = 0;
//     while (s[len])
//         len++;
//     return {(U16 *)s, len};
// #elif defined WCHAR_UTF32
//     if (!s)
//         return {(U32 *)L"", 0};
//     U64 len = 0;
//     while (s[len])
//         len++;
//     return {(U32 *)s, len};
// #endif
// }

U64 wstring_push_wstring(StringBuilder *base, Str16 push)
{
    if (push.size == 0)
        return 0;

    str8_growby(base, push.size * 3 + 1);
    U8 *b_ptr = base->v + base->size;

    U16 *in_ptr = push.v;
    U16 *in_end = push.v + push.size;
    U64 out_pos = 0;
    U64 written = 0;

    UnicodeDecode decode = {};
    for (; in_ptr < in_end; in_ptr += decode.size)
    {
        decode = utf16_decode(in_ptr, in_end - in_ptr);
        out_pos += utf8_encode(b_ptr + out_pos, decode.codepoint);
        written++;
    }
    b_ptr[out_pos] = 0;
    base->size += out_pos;
    return written;
}

U64 wstring_push_wchar(StringBuilder *base, wchar push)
{
    str8_growby(base, 3 + 1);
    U8 *b_ptr = base->v + base->size;

    U16 *in_ptr = (U16 *)&push;
    U64 out_pos = 0;

    UnicodeDecode decode = utf16_decode(in_ptr, 1);
    out_pos += utf8_encode(b_ptr + out_pos, decode.codepoint);
    b_ptr[out_pos] = 0;
    base->size += out_pos;
    return 1;
}

Str16 wstring_view_wcstr(const wchar *c, S64 size)
{
    if (!c)
        return {0};
    if (size >= 0)
        return {(U16 *)c, (U64)size};
    U64 len = 0;
    while (len[c] != 0)
        len++;
    return {(U16 *)c, len};
}

Str16 wstring_copy_wcstr(Arena *arena, const wchar *c, S64 size)
{
    if (!c)
        return {0};

    U64 len = 0;
    if (size >= 0)
    {
        len = size;
    }
    else
    {
        while (c[len])
            len++;
    }

    StringBuilder b = {.arena = arena};
    str8_growby(&b, (len + 1) * sizeof(wchar));

    MemoryCopy(b.v, c, len * sizeof(wchar));
    b.size = len;
    ((U16 *)b.v)[b.size] = 0;

    return Str16Cast(b);
}

Str16 str8_copy_wstr(Arena *arena, Str16 c)
{
    StringBuilder b = {.arena = arena};
    str8_growby(&b, (c.size + 1) * sizeof(wchar));
    MemoryCopy(b.v, c.v, c.size * sizeof(wchar));
    b.size = c.size;
    ((U16 *)b.v)[b.size] = 0;
    return Str16Cast(b);
}

B32 str8_cmp_wstring(Str16 str, Str16 match)
{
    if (str.size != match.size)
        return str.size - match.size;

    U64 l = 0, r = 0;
    for (U64 i = 0; i < str.size && i < match.size; i++)
    {
        l += str.v[i];
        r += match.v[i];
    }
    return l - r;
}

Str16 str8_to_wide(Arena *arena, Str8 str)
{
    Str16 res = {.v = 0};
    if (str.size)
    {
        U64 cap = str.size * 2;
        res.v = push_array(arena, cap + 1, U16);

        U8 *in_ptr = str.v;
        U8 *in_end = str.v + str.size;
        U64 size = 0;
        UnicodeDecode decode = {.size = 0};
        for (; in_ptr < in_end; in_ptr += decode.size)
        {
            decode = utf8_decode(in_ptr, in_end - in_ptr);
            size += utf16_encode(res.v + size, decode.codepoint);
        }
        res.v[size] = 0;
        res.size = size;
    }
    return res;
}

Str16 str8_formatw(StringBuilder *base, const wchar *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    U64 size = vswprintf(0, 0, fmt, args) + 1;
    Str16Clear(*base);
    str8_growto(base, size * sizeof(wchar));

    base->size = vswprintf((wchar *)base->v, size, fmt, args);
    base->v[size * sizeof(wchar)] = 0;
    va_end(args);

    return Str16Cast(*base);
}

S64 str8_rfind_wstr(Str16 s, wchar f)
{
    for (U64 i = s.size; i >= 0; i--)
    {
        if (s.v[i] == f)
            return i;
    }
    return -1;
}

U64 str8_replace_wstring(Str16 base, Str16 find, Str16 replace, U64 count)
{
    Assert(find.size == replace.size, "find and replace size should be same for string view replace");
    U64 replaced = 0;

    for (U64 i = 0; i < base.size - find.size + 1;)
    {
        if (count && replaced >= count)
            break;

        U64 found = 0;
        for (U64 j = 0; j < find.size; j++)
        {
            if (base.v[i + j] != find.v[j])
                break;
            found++;
        }
        if (find.size == found)
        {
            MemoryCopy(base.v + i, replace.v, find.size);
            i += find.size;
            replaced++;
        }
        else
        {
            i++;
        }
    }
    return replaced;
}

U64 str8_replace_wcstr(Str16 base, const wchar *find, const wchar *replace, U64 count)
{
    if (!find || !replace || !*find)
        return 0;

    return str8_replace_wstring(base, sv16(find), sv16(replace), count);
}

B32 match_front_wcstr(Str16 base, const wchar *match)
{
    if (!match)
        return 0;

    U64 len_match = 0;
    while (match[len_match])
        len_match++;

    if (len_match > base.size)
        return false;

    for (U64 i = 0; i < len_match; i++)
    {
        if (base.v[i] != match[i])
            return 0;
    }

    return 1;
}

B32 match_end_wcstr(Str16 base, const wchar *match)
{
    if (!match)
        return 0;

    U64 len_match = 0;
    while (match[len_match])
        len_match++;

    if (len_match > base.size)
        return 0;

    for (S64 i = base.size, j = len_match; j >= 0; i--, j--)
    {
        if (base.v[i] != match[j])
            return 0;
    }
    return 1;
}
