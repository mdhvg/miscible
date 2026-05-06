#include <stdarg.h>

#include "base/arena.h"
#include "base/string.h"
#include "base/base_core.h"

// WString make_string_cwstr(const wchar *s)
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

// String make_CStrCast(const char *s)
// {
//     if (!s)
//         return {(U8 *)"", 0};
//     U64 len = 0;
//     while (s[len])
//         len++;
//     return {(U8 *)s, len};
// }

void string_growto(StringBuilder *base, U64 reqcap)
{
    if (reqcap < base->capacity)
        return;

    U64 cap = base->capacity * 2;
    if (cap < reqcap)
        cap = reqcap;

    U8 *buffer = push_array(base->arena, cap + 1, U8);
    if (base->v)
        MemoryCopy(buffer, base->v, base->size);
    base->v = buffer;
    base->capacity = cap;
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
    U8 byte = str[0];
    U8 byte_class = utf8_class[byte >> 3];

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
        U32 v = codepoint - 0x10000;
        str[0] = (U16)(0xd800 + (v >> 10));
        str[1] = (U16)(0xdc00 + (v & 0x3ff));
        inc = 2;
    }
    return inc;
}

UnicodeDecode utf16_decode(U16 *str, U64 max)
{
    UnicodeDecode decode = {1, UTF_INVALID};
    decode.codepoint = str[0];
    decode.size = 1;
    if (max > 1 && str[0] >= 0xD800 && str[0] < 0xDC00 && 0xDC00 <= str[1] && str[1] <= 0xDFFF)
    {
        decode.codepoint = ((str[0] & 0x3FF) << 10) | (str[1] & 0x3FF) + 0x10000;
        decode.size = 2;
    }
    return decode;
}

#if defined WCHAR_UTF16
U64 string_push_wstring(StringBuilder *base, WString push)
{
    if (push.size == 0)
        return 0;

    string_growby(base, push.size * 3 + 1);
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
#elif defined WCHAR_UTF32
U64 string_push_wstring(StringBuild *base, WString push)
{
    if (push.size == 0)
        return 0;

    string_growby(base, push.size * 4 + 1);
    U8 *b_ptr = base->v + base->size;

    U32 *in_ptr = push.v;
    U32 *in_end = push.v + push.size;
    U64 out_pos = 0;
    U64 written = 0;
    for (; in_ptr < in_end; in_ptr += 1)
    {
        out_pos += utf8_encode(b_ptr + out_pos, *in_ptr);
        written++;
    }
    b_ptr[out_pos] = 0;
    base->size += out_pos;
    return written;
}
#endif

#if defined WCHAR_UTF16
U64 string_push_wchar(StringBuilder *base, wchar push)
{
    string_growby(base, 3 + 1);
    U8 *b_ptr = base->v + base->size;

    U16 *in_ptr = (U16 *)&push;
    U64 out_pos = 0;

    UnicodeDecode decode = utf16_decode(in_ptr, 1);
    out_pos += utf8_encode(b_ptr + out_pos, decode.codepoint);
    b_ptr[out_pos] = 0;
    base->size += out_pos;
    return 1;
}
#elif defined WCHAR_UTF32
U64 string_push_wchar(StringBuilder *base, wchar push)
{
    string_growby(base, 4 + 1);
    U8 *b_ptr = base->v + base->size;

    U32 *in_ptr = (U32 *)&push;
    U64 out_pos = 0;

    UnicodeDecode decode = utf32_decode(in_ptr, 1);
    out_pos += utf8_encode(b_ptr + out_pos, decode.codepoint);
    b_ptr[out_pos] = 0;
    base->size += out_pos;
    return 1;
}
#endif

#if defined WCHAR_UTF16
U64 string_push_wcstr(StringBuilder *base, const wchar *push)
{
    if (!push)
        return 0;

    U64 len = 0;
    while (push[len])
        len++;

    WString ws = {(U16 *)push, len};
    return string_push_wstring(base, ws);
}
#elif defined WCHAR_UTF32
U64 string_push_wcstr(StringBuilder *base, const wchar *push)
{
    if (!push)
        return 0;

    U64 len = 0;
    while (push[len])
        len++;

    WString ws = {(U32 *)push, len};
    return string_push_wstring(base, push);
}
#endif

U64 string_push_string(StringBuilder *base, String push)
{
    if (!push.size)
        return 0;
    string_growby(base, push.size);
    MemoryCopy(base->v + base->size, push.v, push.size);
    base->size += push.size;
    base->v[base->size] = 0;
    return push.size;
}

U64 string_push_char(StringBuilder *base, char push)
{
    string_growby(base, 1 + 1);

    base->v[base->size] = push;
    base->size += 1;
    base->v[base->size] = 0;
    return 1;
}

U64 string_push_cstr(StringBuilder *base, const char *push)
{
    if (!push)
        return 0;

    U64 len = 0;
    while (push[len])
        len++;

    String s = {(U8 *)push, len};
    return string_push_string(base, s);
}

U64 string_assign_string(StringBuilder *base, String push)
{
    string_pop_to(base, 0);
    string_growto(base, push.size);
    MemoryCopy(base->v, push.v, push.size);
    base->size = push.size;
    base->v[base->size] = 0;
    return base->size;
}

WString string_view_wcstr(const wchar *c)
{
    U64 len = 0;
    while (len[c] != 0)
        len++;
    return {(U16 *)c, len};
}

String string_view_cstr(const char *c)
{
    U64 len = 0;
    while (len[c] != 0)
        len++;
    return {(U8 *)c, len};
}

WString string_cpy_wcstr(Arena *arena, const wchar *c)
{
    U64 len = 0;
    while (c[len])
        len++;

    StringBuilder b = {.arena = arena};
    string_growby(&b, (len + 1) * sizeof(wchar));
    MemoryCopy(b.v, c, len * sizeof(wchar));
    b.size = len;
    ((U16 *)b.v)[b.size] = 0;

    return WStringCast(b);
}

String string_cpy_cstr(Arena *arena, const char *c)
{
    U64 len = 0;
    while (c[len])
        len++;

    StringBuilder b = {.arena = arena};
    string_growby(&b, len + 1);
    MemoryCopy(b.v, c, len);
    b.size = len;
    b.v[b.size] = 0;
    return StringCast(b);
}

WString string_cpy_wstr(Arena *arena, WString c)
{
    StringBuilder b = {.arena = arena};
    string_growby(&b, (c.size + 1) * sizeof(wchar));
    MemoryCopy(b.v, c.v, c.size * sizeof(wchar));
    b.size = c.size;
    ((U16 *)b.v)[b.size] = 0;
    return WStringCast(b);
}

String string_cpy_str(Arena *arena, String s)
{
    StringBuilder b = {.arena = arena};
    string_growby(&b, s.size + 1);
    MemoryCopy(b.v, s.v, s.size);
    b.size = s.size;
    b.v[b.size] = 0;
    return StringCast(b);
}

StringBuilder string_empty(Arena *arena, U64 size)
{
    StringBuilder b = {.arena = arena};
    string_growto(&b, size);
    return b;
}

void string_pop_to(StringBuilder *base, U64 size)
{
    base->size = size;
    base->v[size] = 0;
}

B32 string_cmp_wcstr(WString str, const wchar *match, U64 limit)
{
    U64 l = 0, r = 0;
    U64 i = 0;
    for (U64 i = 0; i < limit && i < str.size && match[i]; i++)
    {
        l += str.v[i];
        r += match[i];
    }
    return l - r;
}

B32 string_cmp_cstr(String str, const char *match, U64 limit)
{
    U64 l = 0, r = 0;
    U64 i = 0;
    for (U64 i = 0; i < limit && i < str.size && match[i]; i++)
    {
        l += str.v[i];
        r += match[i];
    }
    return l - r;
}

B32 string_cmp_string(String str, String match)
{
    if (str.size != match.size)
        return str.size - match.size;
    U64 l = 0, r = 0;
    U64 i = 0;
    for (U64 i = 0; i < str.size && i < match.size; i++)
    {
        l += str.v[i];
        r += match.v[i];
    }
    return l - r;
}

// #if defined WCHAR_UTF16
// wchar *str_to_wcstr(Arena *a, String s)
// {
//     wchar *res = NULL;
//     if (s.size)
//     {
//         res = push_array(a, s.size * 2, wchar);
//
//         U8 *in_ptr           = s.v;
//         U8 *in_end           = s.v + s.size;
//         U64 size             = 0;
//         UnicodeDecode decode = {0};
//         for (; in_ptr < in_end; in_ptr += decode.size)
//         {
//             decode = utf8_decode(in_ptr, in_end - in_ptr);
//             size += utf16_encode((U16 *)(res + size), decode.codepoint);
//         }
//         res[size] = 0;
//     }
//     return res;
// }
// #elif defined WCHAR_UTF32
// #endif

void string_formatv(StringBuilder *base, const char *fmt, va_list args)
{
    va_list args2;
    va_copy(args2, args);

    U64 size = vsnprintf(0, 0, fmt, args2) + 1;
    string_clear(*base);
    string_growto(base, size);

    base->size = vsnprintf((char *)base->v, size, fmt, args);
    base->v[size] = 0;

    va_end(args2);
}

WString string_formatw(StringBuilder *base, const wchar *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    U64 size = vswprintf(0, 0, fmt, args) + 1;
    string_clear(*base);
    string_growto(base, size * sizeof(wchar));

    base->size = vswprintf((wchar *)base->v, size, fmt, args);
    base->v[size * sizeof(wchar)] = 0;
    va_end(args);

    return WStringCast(*base);
}

String string_format(StringBuilder *base, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    string_formatv(base, fmt, args);
    va_end(args);

    String result = StringCast(*base);
    return result;
}

// void format_strv(StringBuilder *s, const char *fmt, va_list args)
// {
//     va_list args2;
//     va_copy(args2, args);
//
//     U64 size = vsnprintf(0, 0, fmt, args2) + 1;
//     string_growto(s, size);
//     s->size       = vsnprintf((char *)s->v, size, fmt, args);
//     s->v[s->size] = 0;
//
//     va_end(args2);
// }

// String format_str(StringBuilder *s, const char *fmt, ...)
// {
//     va_list args;
//     va_start(args, fmt);
//     format_strv(s, fmt, args);
//     va_end(args);
//     return *(String *)s;
// }

// const char *format_cstr(StringBuilder *s, const char *fmt, ...)
// {
//     va_list args;
//     va_start(args, fmt);
//     format_strv(s, fmt, args);
//     va_end(args);
//     return CStrCast(StringCast(s));
// }

S64 string_rfind_wstr(WString s, wchar f)
{
    for (U64 i = s.size; i >= 0; i--)
    {
        if (s.v[i] == f)
            return i;
    }
    return -1;
}

U64 string_replace_string(StringBuilder *base, String find, String replace, U64 count)
{
    U64 replaced = 0;
    U64 instances = 0;

    if (find.size == replace.size)
        goto FAST_PATH;
    else if (find.size > replace.size)
        goto MOVE_PATH;
    else
        goto GROW_PATH;

FAST_PATH:
    for (U64 i = 0; i < base->size - find.size + 1;)
    {
        if (count && replaced >= count)
            break;

        U64 found = 0;
        for (U64 j = 0; j < find.size; j++)
        {
            if (base->v[i + j] != find.v[j])
                break;
            found++;
        }
        if (find.size == found)
        {
            MemoryCopy(base->v + i, replace.v, find.size);
            i += find.size;
            replaced++;
        }
        else
        {
            i++;
        }
    }
    goto RES;

MOVE_PATH:
    for (U64 i = 0; i < base->size - find.size + 1;)
    {
        if (count && replaced >= count)
            break;

        U64 found = 0;
        for (U64 j = 0; j < find.size; j++)
        {
            if (base->v[i + j] != find.v[j])
                break;
            found++;
        }
        if (find.size == found)
        {
            MemoryCopy(base->v + i, replace.v, replace.size);
            MemoryCopy(base->v + i + replace.size, base->v + i + find.size, base->size - i - replace.size);
            i += replace.size;
            replaced++;
        }
        else
        {
            i++;
        }
    }
    base->size -= replaced * (find.size - replace.size);
    goto RES;

GROW_PATH:
    for (U64 i = 0; i < base->size - find.size + 1;)
    {
        if (count && instances >= count)
            break;

        U64 found = 0;
        for (U64 j = 0; j < find.size; j++)
        {
            if (base->v[i + j] != find.v[j])
                break;
            found++;
        }
        if (find.size == found)
        {
            instances += 1;
            i += find.size;
        }
        else
        {
            i++;
        }
    }

    string_growby(base, instances * (replace.size - find.size));
    base->size += instances * (replace.size - find.size);

    for (U64 i = 0; i < base->size - find.size;)
    {
        if (count && replaced >= count)
            break;

        U64 found = 0;
        for (U64 j = 0; j < find.size; j++)
        {
            if (base->v[i + j] != find.v[j])
                break;
            found++;
        }
        if (find.size == found)
        {
            MemoryCopy(base->v + i + replace.size, base->v + i + find.size, base->size - i - find.size + 1);
            MemoryCopy(base->v + i, replace.v, replace.size);
            i += replace.size;
            replaced++;
        }
        else
        {
            i++;
        }
    }
    goto RES;

RES:
    base->v[base->size] = 0;
    return replaced;
}

U64 string_replace_wstring(WString base, WString find, WString replace, U64 count)
{
    Assert(find.size == replace.size, "find and replace size should be same for string view replace");
    U64 replaced = 0;
    U64 instances = 0;

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

U64 string_replace_cstr(StringBuilder *base, const char *find, const char *replace, U64 count)
{
    if (!find || !replace || !*find)
        return 0;

    return string_replace_string(base, sv(find), sv(replace), count);
}

U64 string_replace_wcstr(WString base, const wchar *find, const wchar *replace, U64 count)
{
    if (!find || !replace || !*find)
        return 0;

    return string_replace_wstring(base, sv(find), sv(replace), count);
}

B32 match_front_wcstr(WString base, const wchar *match)
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

B32 match_end_wcstr(WString base, const wchar *match)
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

B32 match_front_cstr(String base, const char *match)
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

B32 match_end_cstr(String base, const char *match)
{
    if (!match)
        return 0;

    U64 len_match = 0;
    while (match[len_match])
        len_match++;

    if (len_match > base.size)
        return 0;

    for (S64 i = base.size - 1, j = len_match - 1; j >= 0; i--, j--)
    {
        if (base.v[i] != match[j])
            return 0;
    }
    return 1;
}
