#pragma once
#include "base/base_core.h"
#include "base/arena.h"
#include "base/array.h"

// Reference: https://youtu.be/bUOOaXf9qIM?t=2989

#define UTF_INVALID 0xFFFD

struct String
{
    U8 *v;
    U64 size;
    U64 capacity;
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
    U64 capacity;
};

struct StringArray
{
    _DynamicArrayHeader_;
    String *v;
};

String string_from_to(String in, U64 start, U64 end);

WString make_string_cwstr(const wchar *s);
String make_string_cstr(const char *s);

void string_wstr16(Arena *arena, String *in, WString ws);
void string_wchar16(Arena *arena, String *in, wchar wc);
void string_cwstr16(Arena *arena, String *in, const wchar *wc);
void string_wstr32(Arena *arena, String *in, WString ws);
void string_wchar32(Arena *arena, String *in, wchar wc);
void string_cwstr32(Arena *arena, String *in, const wchar *wc);
void string_str(Arena *arena, String *in, String s);
void string_char(Arena *arena, String *in, char c);
void string_cstr(Arena *arena, String *in, const char *c);

String cwstr_cpy(Arena *arena, const wchar *c);
String cstr_cpy(Arena *arena, const char *c);
String str_cpy(Arena *arena, String c);

char *str_to_cstr(String c);
wchar *str_to_wcstr(Arena *a, String s);

String string_format(Arena *arena, const char *fmt, ...);
WString string_formatw(Arena *arena, const wchar *fmt, ...);

StringBuilder string_empty(Arena *arena, U64 size = 1);
String format_str(StringBuilder *s, const char *fmt, ...);
const char *format_cstr(StringBuilder *s, const char *fmt, ...);

bool match_end(const char *s, const char *match);
bool match_front(const char *s, const char *match);
bool match_end(const wchar *s, const wchar *match);
bool match_front(const wchar *s, const wchar *match);

#if defined(WCHAR_UTF16)
#define string_wstr(a, b, s)  string_wstr16(a, b, s)
#define string_wchar(a, b, s) string_wchar16(a, b, s)
#define string_cwstr(a, b, s) string_cwstr16(a, b, s)
#elif defined(WCHAR_UTF32)
#define string_wstr(a, b, s)  string_wstr32(a, b, s)
#define string_wchar(a, b, s) string_wchar32(a, b, s)
#define string_cwstr(a, b, s) string_cwstr32(a, b, s)
#endif

#if defined(__cplusplus)
inline void string(Arena *a, String *b, WString x)
{
    string_wstr(a, b, x);
}
inline void string(Arena *a, String *b, wchar x)
{
    string_wchar(a, b, x);
}
inline void string(Arena *a, String *b, const wchar *x)
{
    string_cwstr(a, b, x);
}
inline void string(Arena *a, String *b, String x)
{
    string_str(a, b, x);
}
inline void string(Arena *a, String *b, char x)
{
    string_char(a, b, x);
}
inline void string(Arena *a, String *b, const char *x)
{
    string_cstr(a, b, x);
}

//inline String s_cpy(Arena *a, WString x)
//{
//	return wstr_cpy(a, x);
//}
//inline String s_cpy(Arena *a, wchar x)
//{
//	return wchar_cpy(a, x);
//}
inline String s_cpy(Arena *a, const wchar *x)
{
    return cwstr_cpy(a, x);
}
inline String s_cpy(Arena *a, String x)
{
    return str_cpy(a, x);
}
//inline String s_cpy(Arena *a, char x)
//{
//	return char_cpy(a, x);
//}
inline String s_cpy(Arena *a, const char *x)
{
    return cstr_cpy(a, x);
}

inline WString S(const wchar *x)
{
    return make_string_cwstr(x);
}
inline String S(const char *x)
{
    return make_string_cstr(x);
}
#else // __cplusplus
#define string(arena, in, x)   \
    _Generic((x),              \
        WString: string_wstr,  \
        wchar: string_wchar,   \
        wchar *: string_cwstr, \
        String: string_str,    \
        char: string_char,     \
        char *: string_cstr,   \
        default: string_cstr)(arena, in, x)

#define s_cpy(arena, x)     \
    _Generic((x),           \
        WString: wstr_cpy,  \
        wchar: wchar_cpy,   \
        wchar *: cwstr_cpy, \
        String: str_cpy,    \
        char: char_cpy,     \
        char *: cstr_cpy,   \
        default: cstr_cpy)(arena, x)

#define S(x)                        \
    _Generic((x),                   \
        wchar *: make_string_cwstr, \
        char *: make_string_cstr,   \
        default: make_string_cstr)(x)
#endif // __cplusplus
