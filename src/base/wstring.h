// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include "base/string.h"

typedef struct Str16 Str16;
struct Str16
{
    U16 *v;
    U64 size;
};

#define Str16Spr(s)   (s).size, (s).v
#define Str16Cast(x)  *(Str16 *)(&(x))
#define Str16Clear(x) ((x).size = 0, (x).v[0] = 0)
#define WCStrCast(s)  (const wchar *)((s).v)

MSCBL_API U64 str16_push_wstring(StringBuilder *base, Str16 str);
MSCBL_API U64 str16_push_wcstr(StringBuilder *base, const wchar *str);
MSCBL_API U64 str16_push_wchar(StringBuilder *base, wchar chr);

MSCBL_API Str16 str16_copy_wstring(Arena *arena, Str16 str);
MSCBL_API Str16 str16_copy_wcstr(Arena *arena, const wchar *str);
MSCBL_API Str16 str16_copy_wchar(Arena *arena, wchar c);

MSCBL_API Str16 str16_view_wcstr(const wchar *c, S64 size);

MSCBL_API StringBuilder str16_empty(Arena *arena, CppOpt(U64 size, 1));

// NOTE: can break surrogate pairs
MSCBL_API inline Str16 str16_from_to(Str16 base, U64 from, U64 to)
{
    if (from > base.size)
        from = base.size;
    if (to > base.size)
        to = base.size;

    Str16 result = {.v = base.v + from};

    if (from > to)
        result.size = 0;
    else
        result.size = to - from;

    return result;
}
// NOTE: can break surrogate pairs
MSCBL_API inline Str16 str16_from(Str16 base, U64 from)
{
    return str16_from_to(base, from, base.size);
}

MSCBL_API void str16_formatv(StringBuilder *base, const wchar *fmt, va_list args);
MSCBL_API Str16 str16_format(StringBuilder *base, const wchar *fmt, ...);

MSCBL_API B32 str16_cmp(Str16 str, Str16 match, CppOpt(StringMatchFlag flags, 0));

/*
 * Find and replace
 *
 * @param base     The Str16 to modify (in/out).
 * @param find     The substring to search for (null-terminated).
 * @param replace  The replacement string (null-terminated).
 * @param count    Maximum number of replacements (0 = replace all).
 * @return         Number of replacements actually performed.
 */
MSCBL_API U64 str16_replace(StringBuilder *base, Str16 find, Str16 replace, CppOpt(U64 count, 0), CppOpt(StringMatchFlag flags, 0));

MSCBL_API B32 str16_find(Str16 str, Str16 match, CppOpt(StringMatchFlag flags, 0), CppOpt(S64 *loc, 0));
MSCBL_API B32 str16_rfind(Str16 str, Str16 match, CppOpt(StringMatchFlag flags, 0), CppOpt(S64 *loc, 0));

MSCBL_API B32 str16_match_beg(Str16 str, Str16 match, CppOpt(StringMatchFlag flags, 0));
MSCBL_API B32 str16_match_end(Str16 str, Str16 match, CppOpt(StringMatchFlag flags, 0));

MSCBL_API Str16 str8_to_str16(Arena *arena, Str8 str);

#if LANG_CPP
inline U64 str16_push(StringBuilder *base, Str16 push)
{
    return str16_push_wstring(base, push);
}
inline U64 str16_push(StringBuilder *base, const wchar *push)
{
    return str16_push_wcstr(base, push);
}
inline U64 str16_push(StringBuilder *base, wchar push)
{
    return str16_push_wchar(base, push);
}

inline Str16 str16_copy(Arena *arena, Str16 x)
{
    return str16_copy_wstring(arena, x);
}
inline Str16 str16_copy(Arena *arena, const wchar *x)
{
    return str16_copy_wcstr(arena, x);
}
inline Str16 str16_copy(Arena *arena, wchar x)
{
    return str16_copy_wchar(arena, x);
}

inline Str16 sv16(const wchar *x, S64 size = -1)
{
    return str16_view_wcstr(x, size);
}
#else
#define str16_push(base, x) _Generic((x), \
    Str16: str16_push_wstring,            \
    wchar *: str16_push_wcstr,            \
    wchar: str16_push_wchar)(base, x)

#define str16_copy(base, x) _Generic((x), \
    Str16: str16_copy_string,             \
    wchar *: str16_copy_wcstr,            \
    wchar: str16_copy_wchar)(base, x)

#define sv16(x, size) _Generic((x), wchar *: str16_view_wcstr)(x, size)
#endif

// Utilities

#define sv16_lit(x) sv16(x, sizeof(x))

inline void str16_path_join(StringBuilder *path, Str16 part)
{
    if (!str16_match_end(Str16Cast(path), sv16(L"\\", 1)) && !str16_match_end(Str16Cast(path), sv16(L"/", 1)))
        str16_push(path, sv16(L"\\", 1));
    str16_push(path, part);
}

inline StringBuilder str16_init(Arena *arena, Str16 init)
{
    StringBuilder base = str16_empty(arena, init.size);
    str16_push(&base, init);
    return base;
}
