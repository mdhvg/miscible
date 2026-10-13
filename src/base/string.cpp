// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include <cstring>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "base/log.h"
#include "base/arena.h"
#include "base/string.h"

// String make_CStrCast(const char *s)
// {
//     if (!s)
//         return {(U8 *)"", 0};
//     U64 len = 0;
//     while (s[len])
//         len++;
//     return {(U8 *)s, len};
// }

void str8_growto(StringBuilder *base, U64 reqcap)
{
    if (reqcap <= base->capacity)
        return;

    U64 new_cap = base->capacity * 2;
    if (new_cap < reqcap)
        new_cap = reqcap;

    // U8 *buffer = push_array(base->arena, new_cap + 1, U8);
    // if (base->v)
    //     MemoryCopy(buffer, base->v, base->size);
    base->v = (U8 *)arena_resize(base->arena, base->v, base->capacity + 1, new_cap + 1);
    base->capacity = new_cap;
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
    UnicodeDecode result = {.size = 1, .codepoint = UTF8_INVALID};
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
        if (3 < max)
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
    if (codepoint == UTF8_INVALID)
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
    UnicodeDecode decode = {1, UTF8_INVALID};
    decode.codepoint = str[0];
    decode.size = 1;
    if (max > 1 && str[0] >= 0xD800 && str[0] < 0xDC00 && 0xDC00 <= str[1] && str[1] <= 0xDFFF)
    {
        decode.codepoint = ((str[0] & 0x3FF) << 10) | (str[1] & 0x3FF) + 0x10000;
        decode.size = 2;
    }
    return decode;
}

U64 str8_push_string(StringBuilder *base, Str8 push)
{
    if (!push.size)
        return 0;
    str8_growby(base, push.size);
    MemoryCopy(base->v + base->size, push.v, push.size);
    base->size += push.size;
    base->v[base->size] = 0;
    return push.size;
}

U64 str8_push_char(StringBuilder *base, char push)
{
    str8_growby(base, 1 + 1);

    base->v[base->size] = push;
    base->size += 1;
    base->v[base->size] = 0;
    return 1;
}

U64 str8_push_cstr(StringBuilder *base, const char *push)
{
    if (!push)
        return 0;

    U64 len = 0;
    while (push[len])
        len++;

    Str8 s = {(U8 *)push, len};
    return str8_push_string(base, s);
}

U64 str8_assign_string(StringBuilder *base, Str8 push)
{
    str8_pop_to(base, 0);
    str8_growto(base, push.size);
    MemoryCopy(base->v, push.v, push.size);
    base->size = push.size;
    base->v[base->size] = 0;
    return base->size;
}

Str8 str8_view_cstr(const char *c, S64 size)
{
    if (!c)
        return {0};
    if (size >= 0)
        return {(U8 *)c, (U64)size};
    U64 len = 0;
    while (len[c] != 0)
        len++;
    return {(U8 *)c, len};
}

Str8 str8_copy_cstr(Arena *arena, const char *c)
{
    if (!c) return {0};

    U64 len = 0;
    while (c[len]) len++;

    StringBuilder b = {.arena = arena};
    str8_growby(&b, len + 1);

    MemoryCopy(b.v, c, len);
    b.size = len;
    b.v[b.size] = 0;

    return Str8Cast(b);
}

Str8 str8_copy_string(Arena *arena, Str8 s)
{
    StringBuilder b = {.arena = arena};
    str8_growby(&b, s.size + 1);
    MemoryCopy(b.v, s.v, s.size);
    b.size = s.size;
    b.v[b.size] = 0;
    return Str8Cast(b);
}

StringBuilder str8_empty(Arena *arena, U64 size)
{
    StringBuilder b = {.arena = arena};
    str8_growto(&b, size);
    return b;
}

void str8_pop_to(StringBuilder *base, U64 size)
{
    base->size = size;
    base->v[size] = 0;
}

B32 str8_cmp(Str8 str, Str8 match, StringMatchFlag flags)
{
    if (str.size != match.size) return 0;

    if (flags & StringMatch_IgnoreCase)
    {
        for (S64 i = 0; i < str.size; i++)
        {
            U8 b1 = str.v[i];
            U8 b2 = match.v[i];

            if (b1 >= 'A' && b1 <= 'Z') b1 += 32;
            if (b2 >= 'A' && b2 <= 'Z') b2 += 32;

            if (b1 != b2) return 0;
        }
    }

    return memcmp(str.v, match.v, str.size) == 0;
}

void str8_formatv(StringBuilder *base, const char *fmt, va_list args)
{
    va_list args2;
    va_copy(args2, args);
    U64 size = vsnprintf(0, 0, fmt, args2) + 1;
    va_end(args2);

    if (base->arena)
    {
        Str8Clear(*base);
        str8_growto(base, size);
    }

    if (size > base->capacity)
        size = base->capacity;

    base->size = vsnprintf((char *)base->v, size, fmt, args);
    base->v[size] = 0;
}

Str8 str8_format(StringBuilder *base, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    str8_formatv(base, fmt, args);
    va_end(args);

    Str8 result = Str8Cast(*base);
    return result;
}

// void format_strv(StringBuilder *s, const char *fmt, va_list args)
// {
//     va_list args2;
//     va_copy(args2, args);
//
//     U64 size = vsnprintf(0, 0, fmt, args2) + 1;
//     str8_growto(s, size);
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
//     return CStrCast(Str8Cast(s));
// }

B32 str8_find(Str8 str, Str8 match, StringMatchFlag flags, S64 *loc)
{
    if (!str.size || !match.size || str.size < match.size) return 0;

    for (S64 str_idx = 0; str_idx <= str.size - match.size; str_idx++)
    {
        S64 match_idx = 0;
        while (match_idx < match.size)
        {
            U8 b1 = str.v[str_idx + match_idx];
            U8 b2 = match.v[match_idx];

            if (b1 != b2)
            {
                if (flags & StringMatch_IgnoreCase)
                {
                    if (b1 >= 'A' && b1 <= 'Z') b1 += 32;
                    if (b2 >= 'A' && b2 <= 'Z') b2 += 32;

                    if (b1 != b2) break;
                }
                else
                {
                    break;
                }
            }
            match_idx++;
        }

        if (match_idx == match.size)
        {
            if (loc) *loc = str_idx;
            return 1;
        }
    }

    return 0;
}

B32 str8_rfind(Str8 str, Str8 match, StringMatchFlag flags, S64 *loc)
{
    if (!str.size || !match.size || str.size < match.size) return 0;

    for (S64 str_idx = str.size - match.size; str_idx >= 0; str_idx--)
    {
        S64 match_idx = 0;
        while (match_idx < match.size)
        {
            U8 b1 = str.v[str_idx + match_idx];
            U8 b2 = match.v[match_idx];

            if (b1 != b2)
            {
                if (flags & StringMatch_IgnoreCase)
                {
                    if (b1 >= 'A' && b1 <= 'Z') b1 += 32;
                    if (b2 >= 'A' && b2 <= 'Z') b2 += 32;

                    if (b1 != b2) break;
                }
                else
                {
                    break;
                }
            }
            match_idx++;
        }

        if (match_idx == match.size)
        {
            if (loc) *loc = str_idx;
            return 1;
        }
    }

    return 0;
}

// S64 str8_find_cstr(String s, const char *f)
// {
//     for (U64 i = 0; i < s.size; i++)
//     {
//         if (match_front(s, f))
//         {
//             return i;
//         }
//     }
//     return -1;
// }
//
// S64 str8_rfind(String s, String f)
// {
//     for (U64 i = s.size; i >= 0; i--)
//     {
//     }
//     return -1;
// }

U64 str8_replace(StringBuilder *base, Str8 find, Str8 replace, U64 count, StringMatchFlag flags)
{
    if (!base || base->size == 0 || find.size == 0 || base->size < find.size) return 0;

    U64 replaced = 0;
    U64 instances = 0;
    U64 size_diff = 0;
    U64 orig_size = base->size;

    if (find.size == replace.size)
        goto FAST_PATH;
    else if (find.size > replace.size)
        goto MOVE_PATH;
    else
        goto GROW_PATH;

FAST_PATH:
    for (U64 i = 0; i < base->size - find.size + 1;)
    {
        if (count && replaced >= count) break;

        U64 found = 0;
        for (U64 j = 0; j < find.size; j++)
        {
            U8 b1 = base->v[i + j];
            U8 b2 = find.v[j];

            if (flags & StringMatch_IgnoreCase)
            {
                if (b1 >= 'A' && b1 <= 'Z') b1 += 32;
                if (b2 >= 'A' && b2 <= 'Z') b2 += 32;
            }

            if (b1 != b2) break;
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
        if (count && replaced >= count) break;

        U64 found = 0;
        for (U64 j = 0; j < find.size; j++)
        {
            U8 b1 = base->v[i + j];
            U8 b2 = find.v[j];

            if (flags & StringMatch_IgnoreCase)
            {
                if (b1 >= 'A' && b1 <= 'Z') b1 += 32;
                if (b2 >= 'A' && b2 <= 'Z') b2 += 32;
            }

            if (b1 != b2) break;
            found++;
        }
        if (find.size == found)
        {
            MemoryCopy(base->v + i, replace.v, replace.size);
            MemoryCopy(base->v + i + replace.size, base->v + i + find.size, base->size - (i + find.size));
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
        if (count && instances >= count) break;

        U64 found = 0;
        for (U64 j = 0; j < find.size; j++)
        {
            U8 b1 = base->v[i + j];
            U8 b2 = find.v[j];

            if (flags & StringMatch_IgnoreCase)
            {
                if (b1 >= 'A' && b1 <= 'Z') b1 += 32;
                if (b2 >= 'A' && b2 <= 'Z') b2 += 32;
            }

            if (b1 != b2) break;
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

    if (instances == 0) goto RES;

    size_diff = instances * (replace.size - find.size);
    str8_growby(base, size_diff);

    base->size += size_diff;

    for (S64 i = (S64)base->size - (S64)size_diff - (S64)find.size; i < base->size - find.size; i--)
    {
        U64 found = 0;
        for (U64 j = 0; j < find.size; j++)
        {
            U8 b1 = base->v[i + j];
            U8 b2 = find.v[j];

            if (flags & StringMatch_IgnoreCase)
            {
                if (b1 >= 'A' && b1 <= 'Z') b1 += 32;
                if (b2 >= 'A' && b2 <= 'Z') b2 += 32;
            }

            if (b1 != b2) break;
            found++;
        }
        if (find.size == found)
        {
            MemoryCopy(base->v + i + replace.size, base->v + i + find.size, base->size - (i + find.size) - size_diff + (replaced * (replace.size - find.size)));
            MemoryCopy(base->v + i, replace.v, replace.size);

            replaced++;
            i -= (replace.size - 1);
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

B32 str8_match_beg(Str8 str, Str8 match, StringMatchFlag flags)
{
    if (!match.size) return 0;

    U64 len_match = 0;
    while (match.v[len_match]) len_match++;

    if (len_match > str.size) return false;

    for (U64 i = 0; i < len_match; i++)
    {
        if (str.v[i] != match.v[i]) return 0;
    }

    return 1;
}

B32 str8_match_end(Str8 str, Str8 match, StringMatchFlag flags)
{
    if (!match.size) return 0;

    U64 len_match = 0;
    while (match.v[len_match]) len_match++;

    if (len_match > str.size) return 0;

    for (S64 i = str.size - 1, j = len_match - 1; j >= 0; i--, j--)
    {
        if (str.v[i] != match.v[j]) return 0;
    }
    return 1;
}

// clang-format off
global_v U8 integer_symbol_reverse[128] =
{
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
};
// clang-format on

S8 str8_get_sign(Str8 *str)
{
    S8 ret = 1;
    for (U8 **i = &str->v; *i < str->v + str->size && **i == '-'; ((*i)++, str->size--))
    {
        ret *= -1;
    }
    return ret;
}

U64 str8_to_u64(Str8 str, U32 radix)
{
    if (radix <= 1 || radix > 16)
        return 0;

    U64 x = 0;
    for (U64 i = 0; i < str.size; i++)
    {
        x *= radix;
        x += integer_symbol_reverse[str.v[i] & 0x7F];
    }
    return x;
}

F64 str8_to_f64(Str8 str)
{
    char buffer[64];
    MemoryCopy(buffer, str.v, str.size);
    buffer[str.size] = 0;
    char *__end = NULL;
    return strtod(buffer, &__end);
    // S8 sign = str8_get_sign(&str);
    //
    // F64 x = 0;
    // U8 fraction = 0;
    // for (U64 i = 0; i < str.size; i++)
    // {
    //     if (str.v[i] == '.')
    //     {
    //         fraction = 1;
    //         continue;
    //     }
    //     if (!fraction)
    //     {
    //         x *= 10;
    //         x += integer_symbol_reverse[str.v[i] & 0x7F];
    //     }
    //     else
    //     {
    //         x += pow(0.1, fraction) * integer_symbol_reverse[str.v[i] & 0x7F];
    //         fraction++;
    //     }
    // }
    // return sign * x;
}

Str8 str8_month(Month month)
{
    switch (month)
    {
    case Month_Jan: return sv8("JAN");
    case Month_Feb: return sv8("FEB");
    case Month_Mar: return sv8("MAR");
    case Month_Apr: return sv8("APR");
    case Month_May: return sv8("MAY");
    case Month_Jun: return sv8("JUN");
    case Month_Jul: return sv8("JUL");
    case Month_Aug: return sv8("AUG");
    case Month_Sep: return sv8("SEP");
    case Month_Oct: return sv8("OCT");
    case Month_Nov: return sv8("NOV");
    case Month_Dec: return sv8("DEC");
    default: return sv8("Err");
    }
}

Str8 str8_byte(ByteUnit unit)
{
    switch (unit)
    {
    case Byte: return sv8("B");
    case KiByte: return sv8("KB");
    case MiByte: return sv8("MB");
    case GiByte: return sv8("GB");
    case TiByte: return sv8("TB");
    case PiByte: return sv8("PB");
    default: return sv8("Err");
    }
}

#if OS_WIN32
#include "base/wstring.cpp"
#endif
