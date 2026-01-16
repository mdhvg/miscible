#include <cstdio>
#include <stdarg.h>

#include "base/string.h"
#include "base/arena.h"
#include "string.h"

String string_from_to(String in, U64 start, U64 end)
{
    String r = {0};
    r.size   = end - start;
    r.v      = in.v + start;
    return r;
}

WString make_string_cwstr(const wchar *s)
{

#if defined WCHAR_UTF16
    if (!s) return {(U16 *)L"", 0, 0};
    U64 ln = 0;
    while (s[ln]) ln++;
    return {(U16 *)s, ln, ln};
#elif defined WCHAR_UTF32
    if (!s) return {(U32 *)L"", 0, 0};
    U64 ln = 0;
    while (s[ln]) ln++;
    return {(U32 *)s, ln, ln};
#endif
}

String make_string_cstr(const char *s)
{
    if (!s) return {(U8 *)"", 0, 0};
    U64 ln = 0;
    while (s[ln]) ln++;
    return {(U8 *)s, ln, ln};
}

void string_grow(Arena *arena, String *in, U64 capacity)
{
    if (capacity <= in->capacity) return;

    U64 cap = in->capacity * 2;
    if (cap < capacity) cap = capacity;
    if (cap < 256) cap = 256;

    U8 *buffer = push_array(arena, cap, U8);
    if (in->v)
    {
        MemoryCopy(buffer, in->v, in->size);
    }
    in->v        = buffer;
    in->capacity = cap;
}

struct UnicodeDecode
{
    U32 size;
    U32 codepoint;
};

U64 utf8_encode(U8 *ptr, U32 codepoint)
{
    if (codepoint <= 0x7f)
    {
        ptr[0] = (U8)codepoint;
        return 1;
    }
    else if (codepoint <= 0x7ff)
    {
        ptr[0] = (U8)(0xc0 | ((codepoint >> 6) & 0x1f));
        ptr[1] = (U8)(0x80 | (codepoint & 0x3f));
        return 2;
    }
    else if (codepoint <= 0xffff)
    {
        ptr[0] = (U8)(0xe0 | ((codepoint >> 12) & 0x0f));
        ptr[1] = (U8)(0x80 | ((codepoint >> 6) & 0x3f));
        ptr[2] = (U8)(0x80 | (codepoint & 0x3f));
        return 3;
    }
    else if (codepoint <= 0x10ffff)
    {
        ptr[0] = (U8)(0xf0 | ((codepoint >> 18) & 0x07));
        ptr[1] = (U8)(0x80 | ((codepoint >> 12) & 0x3f));
        ptr[2] = (U8)(0x80 | ((codepoint >> 6) & 0x3f));
        ptr[3] = (U8)(0x80 | (codepoint & 0x3f));
        return 4;
    }
    else
    {
        ptr[0] = '?';
        return 1;
    }
}

U8 utf8_class[32] = {
    1, 1, 1, 1,
    1, 1, 1, 1,
    1, 1, 1, 1,
    1, 1, 1, 1,
    0, 0, 0, 0,
    0, 0, 0, 0,
    2, 2, 2, 2,
    3, 3, 4, 5};

UnicodeDecode utf8_decode(U8 *str, U64 max)
{
    UnicodeDecode result = {1, UTF_INVALID};
    U8 byte              = str[0];
    U8 byte_class        = utf8_class[byte >> 3];

    switch (byte_class)
    {
    case 1:
        result.codepoint = byte;
        break;
    case 2:
        if (1 < max)
        {
            U8 cont_byte = str[1];
            if (utf8_class[cont_byte >> 3] == 0)
            {
                result.codepoint = (byte & 0x1f) << 6;
                result.codepoint |= (cont_byte & 0x3f);
                result.size = 2;
            }
        }
        break;
    case 3:
        if (2 < max)
        {
            U8 cont_byte[2] = {str[1], str[2]};
            if (utf8_class[cont_byte[0] >> 3] == 0 &&
                utf8_class[cont_byte[1] >> 3] == 0)
            {
                result.codepoint = (byte & 0x0f) << 12;
                result.codepoint |= ((cont_byte[0] & 0x3f) << 6);
                result.codepoint |= (cont_byte[1] & 0x3f);
                result.size = 3;
            }
        }
        break;
    case 4:
        if (2 < max)
        {
            U8 cont_byte[3] = {str[1], str[2], str[3]};
            if (utf8_class[cont_byte[0] >> 3] == 0 &&
                utf8_class[cont_byte[1] >> 3] == 0 &&
                utf8_class[cont_byte[2] >> 3] == 0)
            {
                result.codepoint = (byte & 0x07) << 18;
                result.codepoint |= ((cont_byte[0] & 0x3f) << 12);
                result.codepoint |= ((cont_byte[1] & 0x3f) << 6);
                result.codepoint |= (cont_byte[2] & 0x3f);
                result.size = 4;
            }
        }
        break;
    }

    return result;
}

U64 utf16_encode(U16 *str, U32 codepoint)
{
    U32 inc = 1;
    if (codepoint == UTF_INVALID)
    {
        str[0] = (U16)'?';
    }
    else if (codepoint < 0x10000)
    {
        str[0] = (U16)codepoint;
    }
    else
    {
        U32 v  = codepoint - 0x10000;
        str[0] = (U16)(0xd800 + (v >> 10));
        str[1] = (U16)(0xdc00 + (v & 0x3ff));
        inc    = 2;
    }
    return inc;
}

UnicodeDecode utf16_decode(U16 *str, U64 max)
{
    UnicodeDecode decode = {1, UTF_INVALID};
    decode.codepoint     = str[0];
    decode.size          = 1;
    if (max > 1 && str[0] >= 0xD800 && str[0] < 0xDC00 && 0xDC00 <= str[1] && str[1] <= 0xDFFF)
    {
        decode.codepoint = ((str[0] & 0x3FF) << 10) | (str[1] & 0x3FF) + 0x10000;
        decode.size      = 2;
    }
    return decode;
}

#if defined WCHAR_UTF16
void string_wstr16(Arena *arena, String *in, WString ws)
{
    if (ws.size == 0) return;

    string_grow(arena, in, ws.size * 3 + 1);
    U8 *b_ptr = in->v + in->size;

    U16 *in_ptr          = ws.v;
    U16 *in_end          = ws.v + ws.size;
    U64 out_pos          = 0;
    UnicodeDecode decode = {};
    for (; in_ptr < in_end; in_ptr += decode.size)
    {
        decode = utf16_decode(in_ptr, in_end - in_ptr);
        out_pos += utf8_encode(b_ptr + out_pos, decode.codepoint);
    }
    b_ptr[out_pos] = 0;
}
#elif defined WCHAR_UTF32
void string_wstr32(Arena *arena, String *in, WString ws)
{
    if (ws.size == 0) return;

    string_grow(arena, in, ws.size * 4 + 1);
    U8 *b_ptr = in->v + in->size;

    U32 *in_ptr = ws.v;
    U32 *in_end = ws.v + ws.size;
    U64 out_pos = 0;
    for (; in_ptr < in_end; in_ptr += 1)
    {
        out_pos += utf8_encode(b_ptr + out_pos, *in_ptr);
    }
    b_ptr[out_pos] = 0;
}
#endif

#if defined WCHAR_UTF16
void string_wchar16(Arena *arena, String *in, wchar wc)
{
    WString ws = {(U16 *)&wc, 1};
    string_wstr(arena, in, ws);
}
#elif defined WCHAR_UTF32
#endif

#if defined WCHAR_UTF16
void string_cwstr16(Arena *arena, String *in, const wchar *wc)
{
    if (!wc) return;
    U64 ln = 0;
    while (wc[ln]) ln++;

    WString ws = {(U16 *)wc, ln};
    string(arena, in, ws);
}
#elif defined WCHAR_UTF32
void string_cwstr32(Arena *arena, String *in, const wchar *wc)
{
    if (!wc) return;
    U64 ln = 0;
    while (wc[ln]) ln++;

    WString ws = {(U32 *)wc, ln};
    string(arena, in, ws);
}
#endif

void string_str(Arena *arena, String *in, String s)
{
    if (!s.size) return;
    string_grow(arena, in, s.size);
    MemoryCopy(in->v + in->size, s.v, s.size);
    in->size += s.size;
}

void string_char(Arena *arena, String *in, char c)
{
    String s = {(U8 *)&c, 1};
    string(arena, in, s);
}

void string_cstr(Arena *arena, String *in, const char *c)
{
    if (!c) return;
    U64 ln = 0;
    while (c[ln]) ln++;

    String s = {(U8 *)c, ln};
    string(arena, in, s);
}

String cstr_cpy(Arena *arena, const char *c)
{
    String s = {0};
    string(arena, &s, c);
    return s;
}

String cwstr_cpy(Arena *arena, const wchar *c)
{
    String s = {0};
    string(arena, &s, c);
    return s;
}

String str_cpy(Arena *arena, String c)
{
    String s = {0};
    string(arena, &s, c);
    return s;
}

char *str_to_cstr(String s)
{
    if (s.capacity > s.size)
        s.v[s.size] = 0;
    return (char *)s.v;
}

#if defined WCHAR_UTF16
wchar *str_to_wcstr(Arena *a, String s)
{
    wchar *res = NULL;
    if (s.size)
    {
        res = push_array(a, s.size * 2, wchar);

        U8 *in_ptr           = s.v;
        U8 *in_end           = s.v + s.size;
        U64 size             = 0;
        UnicodeDecode decode = {0};
        for (; in_ptr < in_end; in_ptr += decode.size)
        {
            decode = utf8_decode(in_ptr, in_end - in_ptr);
            size += utf16_encode((U16 *)(res + size), decode.codepoint);
        }
        res[size] = 0;
    }
    return res;
}
#elif defined WCHAR_UTF32
#endif

String string_formatv(Arena *arena, const char *fmt, va_list args)
{
    va_list args2;
    va_copy(args2, args);
    U64 size              = vsnprintf(0, 0, fmt, args2) + 1;
    String result         = {0};
    result.v              = push_array(arena, size, U8);
    result.size           = vsnprintf((char *)result.v, size, fmt, args);
    result.v[result.size] = 0;
    if (result.capacity < size)
        result.capacity = size;
    va_end(args2);
    return result;
}

String string_format(Arena *arena, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    String result = string_formatv(arena, fmt, args);
    va_end(args);
    return result;
}

WString string_formatw(Arena *arena, const wchar *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    U64 size       = vswprintf(0, 0, fmt, args) + 1;
    WString result = {0};
#if defined WCHAR_UTF16
    result.v = push_array(arena, size, U16);
#elif defined WCHAR_UTF32
    result.v = push_array(arena, size, U32);
#endif
    result.size           = vswprintf((wchar *)result.v, size, fmt, args);
    result.capacity       = size;
    result.v[result.size] = 0;
    va_end(args);
    return result;
}

StringBuilder string_empty(Arena *arena, U64 size)
{
    StringBuilder s = {0};
    s.arena         = arena;
    string_grow(arena, (String *)&s, size);
    return s;
}

void format_strv(StringBuilder *s, const char *fmt, va_list args)
{
    va_list args2;
    va_copy(args2, args);

    U64 size = vsnprintf(0, 0, fmt, args2) + 1;
    if (s->capacity < size)
        string_grow(s->arena, (String *)s, size);
    s->size       = vsnprintf((char *)s->v, size, fmt, args);
    s->v[s->size] = 0;

    va_end(args2);
}

String format_str(StringBuilder *s, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    format_strv(s, fmt, args);
    va_end(args);
    return *(String *)s;
}

const char *format_cstr(StringBuilder *s, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    format_strv(s, fmt, args);
    va_end(args);
    return str_to_cstr(*(String *)s);
}

bool match_end(const char *s, const char *match)
{
    if (!s || !match) return false;
    S64 ln_s = 0;
    while (s[ln_s]) ln_s++;
    S64 ln_match = 0;
    while (match[ln_match]) ln_match++;
    if (ln_match > ln_s) return false;
    while (ln_match >= 0)
    {
        if (s[ln_s] != match[ln_match]) return false;
        ln_match--;
        ln_s--;
    }
    return true;
}

bool match_front(const char *s, const char *match)
{
    if (!s || !match) return false;
    S64 ln_s = 0;
    while (s[ln_s]) ln_s++;
    S64 ln_match = 0;
    while (match[ln_match]) ln_match++;
    if (ln_match > ln_s) return false;
    S64 cur = 0;
    while (match[cur])
    {
        if (s[cur] != match[cur])
            return false;
        cur++;
    }
    return true;
}

bool match_end(const wchar *s, const wchar *match)
{
    if (!s || !match) return false;
    S64 ln_s = 0;
    while (s[ln_s]) ln_s++;
    S64 ln_match = 0;
    while (match[ln_match]) ln_match++;
    if (ln_match > ln_s) return false;
    while (ln_match >= 0)
    {
        if (s[ln_s] != match[ln_match]) return false;
        ln_match--;
        ln_s--;
    }
    return true;
}

bool match_front(const wchar *s, const wchar *match)
{
    if (!s || !match) return false;
    S64 ln_s = 0;
    while (s[ln_s]) ln_s++;
    S64 ln_match = 0;
    while (match[ln_match]) ln_match++;
    if (ln_match > ln_s) return false;
    S64 cur = 0;
    while (match[cur])
    {
        if (s[cur] != match[cur])
            return false;
        cur++;
    }
    return true;
}
