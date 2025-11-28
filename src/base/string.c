#include "base/string.h"

String string_from_to(String in, U64 start, U64 end)
{
	String r = {0};
	r.size = end - start;
	r.value = in + start;
	return r;
}

WString make_string_cwstr(const wchar *s)
{
	if (!s) return {L"", 0};
	U64 ln = 0;
	while (s[ln]) ln++;
	return {s, ln};
}

String make_string_cstr(const char *s)
{
	if (!s) return {"", 0};
	U64 ln = 0;
	while (s[ln]) ln++;
	return {s, ln};
}

void string_builder_grow(Arena *arena, StringBuilder *builder, U64 capacity)
{
	if (capacity <= builder->capacity) return;

	U64 cap = builder->capacity * 2;
	if (cap < capacity) cap = capacity;
	if (cap < 256) cap = 256;

	U8 *buffer = push_array(arena, cap, U8);
	if (builder->value)
	{
		MemoryCopy(buffer, builder->value, builder->length);
	}
	builder->value = buffer;
	builder->capacity = cap;
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
		*(ptr++) = 0xc0 | (codepoint >> 6);
		*(ptr++) = 0x80 | (codepoint & 0x3f);
	}
	else
	{
		*(ptr++) = 0xe0 | (codepoint >> 12);
		*(ptr++) = 0x80 | ((codepoint >> 6) & 0x3f);
		*(ptr++) = 0x80 | (codepoint & 0x3f);
	}
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

void string_builder_wstr16(Arena *arena, StringBuilder *builder, WString ws)
{
	if (ws.length == 0) return;

	string_builder_grow(arena, builder, ws.length * 3);
	U8 *b_ptr = builder->value + builder->length;

	U16 *in_ptr = ws.value;
	U16 *in_end = ws.value + ws.length;
	UnicodeDecode decode = {};
	for (; in_ptr < in_end; in_ptr += decode.size)
	{
		decode = utf16_decode(in_ptr, in_end - in_ptr);
		utf8_encode(b_ptr, decode.codepoint);
	}
}

void string_builder_wchar16(Arena *arena, StringBuilder *builder, wchar wc)
{
	WString ws = {(U16 *)&wc, 1};
	string_builder_wstr(arena, builder, ws);
}

void string_builder_cwstr16(Arena *arena, StringBuilder *builder, const wchar *wc)
{
	if (!wc) return;
	U64 ln = 0;
	while (wc[ln]) ln++;

	WString ws = {(U16 *)wc, ln};
	string_builder_(arena, builder, ws);
}

void string_builder_str(Arena *arena, StringBuilder *builder, String s)
{
	if (!s.length) return;
	string_builder_grow(arena, builder, s.length);
	MemoryCopy(builder->value + builder->length, s.value, s.length);
	builder->length += s.length;
}

void string_builder_char(Arena *arena, StringBuilder *builder, char c)
{
	String s = {(U8 *)&c, 1};
	string_builder(arena, builder, s);
}

void string_builder_cstr(Arena *arena, StringBuilder *builder, const char *c)
{
	if (!c) return;
	U64 ln = 0;
	while (c[ln]) ln++;

	String s = {(U8 *)c, ln};
	string_builder(arena, builder, s);
}

String get_string(StringBuilder *builder)
{
	return {builder->value, builder->length};
}

String cstr_cpy(Arena *arena, const char *c)
{
	StringBuilder b = {};
	string_builder(arena, &b, c);
	return get_string(&b);
}

String cwstr_cpy(Arena *arena, const wchar *c)
{
	StringBuilder b = {};
	string_builder(arena, &b, c);
	return get_string(&b);
}