#include <stdarg.h>

#include "base/string.h"
#include "string.h"

String string_from_to(String in, U64 start, U64 end)
{
	String r = {0};
	r.size	 = end - start;
	r.v		 = in.v + start;
	return r;
}

WString make_string_cwstr(const wchar *s)
{
	if (!s) return {(U16 *)L"", 0, 0};
	U64 ln = 0;
	while (s[ln]) ln++;
	return {(U16 *)s, ln, ln};
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
	in->v		 = buffer;
	in->capacity = cap;
}

struct UnicodeDecode
{
	U32 size;
	U32 codepoint;
};

void utf8_encode(U8 *ptr, U32 codepoint)
{
	if (codepoint <= 0x7f)
	{
		*(ptr++) = (U8)codepoint;
	}
	else if (codepoint <= 0x7ff)
	{
		*(ptr++) = (U8)(0xc0 | (codepoint >> 6));
		*(ptr++) = (U8)(0x80 | (codepoint & 0x3f));
	}
	else
	{
		*(ptr++) = (U8)(0xe0 | (codepoint >> 12));
		*(ptr++) = (U8)(0x80 | ((codepoint >> 6) & 0x3f));
		*(ptr++) = (U8)(0x80 | (codepoint & 0x3f));
	}
}

UnicodeDecode utf16_decode(U16 *str, U64 max)
{
	UnicodeDecode decode = {1, UTF_INVALID};
	decode.codepoint	 = str[0];
	decode.size			 = 1;
	if (max > 1 && str[0] >= 0xD800 && str[0] < 0xDC00 && 0xDC00 <= str[1] && str[1] <= 0xDFFF)
	{
		decode.codepoint = ((str[0] & 0x3FF) << 10) | (str[1] & 0x3FF) + 0x10000;
		decode.size		 = 2;
	}
	return decode;
}

void string_wstr16(Arena *arena, String *in, WString ws)
{
	if (ws.size == 0) return;

	string_grow(arena, in, ws.size * 3);
	U8 *b_ptr = in->v + in->size;

	U16 *in_ptr			 = ws.v;
	U16 *in_end			 = ws.v + ws.size;
	UnicodeDecode decode = {};
	for (; in_ptr < in_end; in_ptr += decode.size)
	{
		decode = utf16_decode(in_ptr, in_end - in_ptr);
		utf8_encode(b_ptr, decode.codepoint);
	}
}

void string_wchar16(Arena *arena, String *in, wchar wc)
{
	WString ws = {(U16 *)&wc, 1};
	string_wstr(arena, in, ws);
}

void string_cwstr16(Arena *arena, String *in, const wchar *wc)
{
	if (!wc) return;
	U64 ln = 0;
	while (wc[ln]) ln++;

	WString ws = {(U16 *)wc, ln};
	string(arena, in, ws);
}

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

char *str_to_cstr(String c)
{
	if (c.capacity > c.size)
		c.v[c.size] = 0;
	return (char *)c.v;
}

String string_format(Arena *arena, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	U64 size			  = vsnprintf(0, 0, fmt, args) + 1;
	String result		  = {0};
	result.v			  = push_array(arena, size, U8);
	result.size			  = vsnprintf((char *)result.v, size, fmt, args);
	result.v[result.size] = 0;
	va_end(args);
	return result;
}

StringGrow string_empty(Arena *arena)
{
	StringGrow s = {0};
	s.arena		 = arena;
	string_grow(arena, (String *)&s, 1);
	return s;
}

String format_str(StringGrow *s, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	U64 size = vsnprintf(0, 0, fmt, args) + 1;
	if (s->capacity < size) string_grow(s->arena, (String *)s, size);
	s->size		  = vsnprintf((char *)s->v, size, fmt, args);
	s->v[s->size] = 0;
	va_end(args);
	return *(String *)s;
}

const char *format_cstr(StringGrow *s, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	U64 size = vsnprintf(0, 0, fmt, args) + 1;
	if (s->capacity < size) string_grow(s->arena, (String *)s, size);
	s->size		  = vsnprintf((char *)s->v, size, fmt, args);
	s->v[s->size] = 0;
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