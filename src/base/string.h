#pragma once
#include <stdarg.h>

#include "base/base_core.h"
#include "base/arena.h"

// Reference: https://youtu.be/bUOOaXf9qIM?t=2989

#define UTF_INVALID 0xFFFD

struct String
{
    U8 *v;
    U64 size;
};

struct StringBuilder
{
    U8 *v;
    U64 size;
    U64 capacity;
    Arena *arena;
};

struct WString
{
#if defined WCHAR_UTF16
    U16 *v;
#elif defined WCHAR_UTF32
    U32 *v;
#endif
    U64 size;
};

#if OS_WINDOWS
typedef struct WString OSString;
#elif OS_LINUX
typedef struct String OSString;
#endif

#define StringSpr(s)  ((S32)(s).size), (s).v
#define WStringSpr(s) (s).size, (s).v

#define StringCast(x)  *(String *)(&(x))
#define WStringCast(x) *(WString *)(&(x))
#define CStrCast(s)    (char *)((s).v)
inline const wchar *WCStrCast(WString s)
{
    return (wchar *)((s).v);
}
wchar *WCStrCast(Arena *a, String s);

MSCBL_API void string_growto(StringBuilder *base, U64 reqcap);
MSCBL_API inline void string_growby(StringBuilder *base, U64 by)
{
    string_growto(base, base->size + by);
}

MSCBL_API U64 string_push_wstring(StringBuilder *base, WString push);
MSCBL_API U64 string_push_wchar(StringBuilder *base, wchar push);
MSCBL_API U64 string_push_wcstr(StringBuilder *base, const wchar *push);
MSCBL_API U64 string_push_string(StringBuilder *base, String push);
MSCBL_API U64 string_push_char(StringBuilder *base, char push);
MSCBL_API U64 string_push_cstr(StringBuilder *base, const char *push);
inline U64 string_push(StringBuilder *base, WString push)
{
    return string_push_wstring(base, push);
}
inline U64 string_push(StringBuilder *base, wchar push)
{
    return string_push_wchar(base, push);
}
inline U64 string_push(StringBuilder *base, const wchar *push)
{
    return string_push_wcstr(base, push);
}
inline U64 string_push(StringBuilder *base, String push)
{
    return string_push_string(base, push);
}
inline U64 string_push(StringBuilder *base, char push)
{
    return string_push_char(base, push);
}
inline U64 string_push(StringBuilder *base, const char *push)
{
    return string_push_cstr(base, push);
}

MSCBL_API U64 string_assign_string(StringBuilder *base, String push);
inline U64 string_assign(StringBuilder *base, String x)
{
    return string_assign_string(base, x);
}

MSCBL_API WString string_view_wcstr(const wchar *c, S64 size);
MSCBL_API String string_view_cstr(const char *c, S64 size);
inline WString sv(const wchar *x, S64 size = -1)
{
    return string_view_wcstr(x, size);
}
inline String sv(const char *x, S64 size = -1)
{
    return string_view_cstr(x, size);
}
inline String sv(const unsigned char *x, S64 size = -1)
{
    return string_view_cstr((char *)x, size);
}

MSCBL_API WString string_copy_wcstr(Arena *arena, const wchar *c);
MSCBL_API String string_copy_cstr(Arena *arena, const char *c);
MSCBL_API WString string_copy_wstr(Arena *arena, WString s);
MSCBL_API String string_copy_str(Arena *arena, String s);
inline WString string_copy(Arena *arena, const wchar *x)
{
    return string_copy_wcstr(arena, x);
}
inline String string_copy(Arena *arena, const char *x)
{
    return string_copy_cstr(arena, x);
}
inline String string_copy(Arena *arena, const unsigned char *x)
{
    return string_copy_cstr(arena, (const char *)x);
}
inline WString string_copy(Arena *arena, WString x)
{
    return string_copy_wstr(arena, x);
}
inline String string_copy(Arena *arena, String x)
{
    return string_copy_str(arena, x);
}

#define string_clear(x) ((x).size = 0, (x).v[0] = 0)
MSCBL_API StringBuilder string_empty(Arena *arena, U64 size = 1);
MSCBL_API void string_pop_to(StringBuilder *base, U64 size);
MSCBL_API inline void string_pop_by(StringBuilder *base, U64 count)
{
    return string_pop_to(base, base->size - count);
}

MSCBL_API inline String string_from_to(String base, U64 from, U64 to)
{
    return {.v = base.v + from, .size = to - from};
}
MSCBL_API inline String string_from(String base, U64 from)
{
    return string_from_to(base, from, base.size);
}

inline StringBuilder string_init(Arena *arena, String init)
{
    StringBuilder base = string_empty(arena, init.size);
    string_push(&base, init);
    return base;
}

MSCBL_API B32 string_cmp_wcstr(WString str, const wchar *match, U64 limit);
MSCBL_API B32 string_cmp_cstr(String str, const char *match, U64 limit);
MSCBL_API B32 string_cmp_string(String str, String match);
inline B32 string_cmp(WString str, const wchar *match, U64 limit = 1024)
{
    return string_cmp_wcstr(str, match, limit);
}
inline B32 string_cmp(String str, const char *match, U64 limit = 1024)
{
    return string_cmp_cstr(str, match, limit);
}
inline B32 string_cmp(String str, String match)
{
    return string_cmp_string(str, match);
}

void string_formatv(StringBuilder *base, const char *fmt, va_list args);
MSCBL_API WString string_formatw(StringBuilder *base, const wchar *fmt, ...);
MSCBL_API String string_format(StringBuilder *base, const char *fmt, ...);
MSCBL_API inline const char *format_cstr(StringBuilder *base, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    string_formatv(base, fmt, args);
    va_end(args);
    return CStrCast(*base);
}

MSCBL_API S64 string_rfind_wstr(WString s, wchar f);
inline S64 string_rfind(WString s, wchar f)
{
    return string_rfind_wstr(s, f);
}
/*
 * Find and replace
 *
 * @param base     The StringBuilder to modify (in/out).
 * @param find     The substring to search for (null-terminated).
 * @param replace  The replacement string (null-terminated).
 * @param count    Maximum number of replacements (0 = replace all).
 * @return         Number of replacements actually performed.
 */
MSCBL_API U64 string_replace_cstr(StringBuilder *base, const char *find, const char *replace, U64 count);
/*
 * Find and replace
 *
 * @param base     The WString to modify (in/out).
 * @param find     The substring to search for (null-terminated).
 * @param replace  The replacement string (null-terminated).
 * @param count    Maximum number of replacements (0 = replace all).
 * @return         Number of replacements actually performed.
 */
MSCBL_API U64 string_replace_wcstr(WString base, const wchar *find, const wchar *replace, U64 count);
/*
 * Inline wrapper for string_replace_cstr (uses default count = 0).
 *
 * @param base     The StringBuilder to modify.
 * @param find     Substring to find.
 * @param replace  Replacement string.
 * @param count    Max replacements (default 0 = all).
 * @return         Number of replacements made.
 */
inline U64 string_replace(StringBuilder *base, const char *find, const char *replace, U64 count = 0)
{
    return string_replace_cstr(base, find, replace, count);
}
inline U64 string_replace(WString base, const wchar *find, const wchar *replace, U64 count = 0)
{
    return string_replace_wcstr(base, find, replace, count);
}

MSCBL_API B32 match_front_wcstr(WString base, const wchar *match);
MSCBL_API B32 match_end_wcstr(WString base, const wchar *match);
MSCBL_API B32 match_front_cstr(String base, const char *match);
MSCBL_API B32 match_end_cstr(String base, const char *match);
inline B32 match_front(WString base, const wchar *match)
{
    return match_front_wcstr(base, match);
}
inline B32 match_end(WString base, const wchar *match)
{
    return match_end_wcstr(base, match);
}
inline B32 match_front(String base, const char *match)
{
    return match_front_cstr(base, match);
}
inline B32 match_end(String base, const char *match)
{
    return match_end_cstr(base, match);
}

inline void path_join(StringBuilder *base, String part)
{
    if (!match_end_cstr(StringCast(*base), "/") && !match_end_cstr(StringCast(*base), "\\"))
        string_push(base, "/");
    string_push(base, part);
}

MSCBL_API String month_string(Month month);
MSCBL_API String byte_string(ByteUnit unit);
