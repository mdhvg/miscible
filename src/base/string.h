// Copyright (c) 2025-2026 Madhav Goyalstrinh
// Licensed under the GNU General Public License v3.0 (see LICENSE)

// Reference: https://youtu.be/bUOOaXf9qIM?t=2989

#pragma once
#include <stdarg.h>

#include "base/arena.h"
#include "base/base_core.h"

typedef struct Str8 Str8;
struct Str8
{
    U8 *v;
    U64 size;
};
typedef Str8 *Str8Arr;

typedef struct StringBuilder StringBuilder;
struct StringBuilder
{
    U8 *v;
    U64 size;
    U64 capacity;
    Arena *arena;
};

#define Str8Stack(name, byte_capacity)         \
    char name##_backend_buffer[byte_capacity]; \
    StringBuilder name = {                     \
        .v = (U8 *)name##_backend_buffer,      \
        .size = 0,                             \
        .capacity = (byte_capacity) - 1,       \
        .arena = NULL}

#define Str8Spr(s)   ((S32)(s).size), (s).v
#define Str8Cast(x)  *(Str8 *)(&(x))
#define Str8Clear(x) ((x).size = 0, (x).v[0] = 0)
#define CStrCast(s)  (char *)((s).v)

#define UTF8_INVALID 0xFFFD

MSCBL_API void str8_growto(StringBuilder *base, U64 reqcap);
MSCBL_API inline void str8_growby(StringBuilder *base, U64 by)
{
    str8_growto(base, base->size + by);
}

MSCBL_API U64 str8_push_string(StringBuilder *base, Str8 str);
MSCBL_API U64 str8_push_cstr(StringBuilder *base, const char *str);
MSCBL_API U64 str8_push_char(StringBuilder *base, char chr);

MSCBL_API Str8 str8_copy_string(Arena *arena, Str8 str);
MSCBL_API Str8 str8_copy_cstr(Arena *arena, const char *str);
MSCBL_API Str8 str8_copy_char(Arena *arena, char chr);

MSCBL_API Str8 str8_view_cstr(const char *c, S64 size);

MSCBL_API StringBuilder str8_empty(Arena *arena, CppOpt(U64 size, 1));
MSCBL_API U64 str8_assign(StringBuilder *base, Str8 push);

MSCBL_API void str8_pop_to(StringBuilder *base, U64 size);
MSCBL_API inline void str8_pop_by(StringBuilder *base, U64 count)
{
    str8_pop_to(base, base->size - count);
}

MSCBL_API inline Str8 str8_from_to(Str8 base, U64 from, U64 to)
{
    if (from > base.size)
        from = base.size;
    if (to > base.size)
        to = base.size;

    Str8 result = {.v = base.v + from};

    if (from > to)
        result.size = 0;
    else
        result.size = to - from;

    return result;
}
MSCBL_API inline Str8 str8_from(Str8 base, U64 from)
{
    return str8_from_to(base, from, base.size);
}

MSCBL_API void str8_formatv(StringBuilder *base, const char *fmt, va_list args);
MSCBL_API Str8 str8_format(StringBuilder *base, const char *fmt, ...);

typedef U32 StringMatchFlag;
enum
{
    StringMatch_IgnoreCase = (1 << 0),
};

MSCBL_API B32 str8_cmp(Str8 str, Str8 match, CppOpt(StringMatchFlag flags, 0));

/*
 * Find and replace
 *
 * @param base     The StringBuilder to modify (in/out).
 * @param find     The substring to search for.
 * @param replace  The replacement string.
 * @param count    Maximum number of replacements (0 = replace all).
 * @return         Number of replacements actually performed.
 */
MSCBL_API U64 str8_replace(StringBuilder *base, Str8 find, Str8 replace, CppOpt(U64 count, 0), CppOpt(StringMatchFlag flags, 0));
MSCBL_API B32 str8_find(Str8 str, Str8 match, CppOpt(StringMatchFlag flags, 0), CppOpt(S64 *loc, 0));
MSCBL_API B32 str8_rfind(Str8 str, Str8 match, CppOpt(StringMatchFlag flags, 0), CppOpt(S64 *loc, 0));

MSCBL_API B32 str8_match_beg(Str8 str, Str8 match, CppOpt(StringMatchFlag flags, 0));
MSCBL_API B32 str8_match_end(Str8 str, Str8 match, CppOpt(StringMatchFlag flags, 0));

MSCBL_API S8 str8_get_sign(Str8 *str);
MSCBL_API U64 str8_to_u64(Str8 str, U32 radix);
inline S64 str8_to_s64(Str8 str, U32 radix)
{
    S8 sign = str8_get_sign(&str);
    return (S64)str8_to_u64(str, radix) * sign;
}
inline U32 str8_to_u32(Str8 str, U32 radix)
{
    return (U32)str8_to_u64(str, radix);
}
inline S32 str8_to_s32(Str8 str, U32 radix)
{
    return (S32)str8_to_s64(str, radix);
}

MSCBL_API F64 str8_to_f64(Str8 str);
inline F32 str8_to_f32(Str8 str)
{
    return (F32)str8_to_f64(str);
}

MSCBL_API Str8 str8_month(Month month);
MSCBL_API Str8 str8_byte(ByteUnit unit);

#if LANG_CPP
inline U64 str8_push(StringBuilder *base, Str8 x)
{
    return str8_push_string(base, x);
}
inline U64 str8_push(StringBuilder *base, const char *x)
{
    return str8_push_cstr(base, x);
}
inline U64 str8_push(StringBuilder *base, char x)
{
    return str8_push_char(base, x);
}

inline Str8 str8_copy(Arena *arena, Str8 x)
{
    return str8_copy_string(arena, x);
}
inline Str8 str8_copy(Arena *arena, const unsigned char *x)
{
    return str8_copy_cstr(arena, (const char *)x);
}
inline Str8 str8_copy(Arena *arena, const char *x)
{
    return str8_copy_cstr(arena, x);
}
inline Str8 str8_copy(Arena *arena, char x)
{
    return str8_copy_char(arena, x);
}

inline Str8 sv8(const char *x, S64 size = -1)
{
    return str8_view_cstr(x, size);
}
inline Str8 sv8(const unsigned char *x, S64 size = -1)
{
    return str8_view_cstr((const char *)x, size);
}
#else
#define str8_push(base, x) _Generic((x), \
    String: str8_push_string,            \
    char: str8_push_char,                \
    int: str8_push_char)(base, x)

#define str8_copy(base, x) _Generic((x), \
    String: str8_copy_string,            \
    char: str8_copy_char)(base, x)

#define sv8(x, size) _Generic((x), \
    char *: str8_view_cstr,        \
    unsigned char *: str8_view_cstr)(x, size)
#endif

// Utilities

#define sv8_lit(x) sv8(x, sizeof(x))

inline void str8_path_join(StringBuilder *path, Str8 part)
{
    if (!str8_match_end(Str8Cast(path), sv8("/", 1)))
        str8_push(path, sv8("/", 1));
    str8_push(path, part);
}

inline StringBuilder str8_init(Arena *arena, Str8 init)
{
    StringBuilder base = str8_empty(arena, init.size);
    str8_push(&base, init);
    return base;
}
