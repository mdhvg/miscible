#pragma once

#include "base/core.h"
#include "base/arena.h"
#include "base/array.h"

// Reference: https://youtu.be/bUOOaXf9qIM?t=2989

#define _StringHeader_ \
	struct               \
	{                    \
		U8 *value;         \
		U64 length;        \
	}

#define UTF_INVALID 0xFFFD

struct String
{
	_StringHeader_;
};

struct WString
{
#if defined WCHAR_UTF16
	U16 *value;
#elif defined WCHAR_UTF32
	U32 *value;
#endif

	U64 length;
};

struct StringBuilder
{
	_StringHeader_;
	U64 capacity;
};

struct StringArray
{
	_ArrayHeader_;
	String *v;
};

#if defined WCHAR_UTF16
#define string_builder_wstr(a, b, s)	string_builder_wstr16(a, b, s)
#define string_builder_wchar(a, b, s) string_builder_wchar16(a, b, s)
#define string_builder_cwstr(a, b, s) string_builder_cwstr16(a, b, s)
#elif defined WCHAR_UTF32
#define string_builder_wstr(a, b, s)	string_builder_wstr32(a, b, s)
#define string_builder_wchar(a, b, s) string_builder_wchar32(a, b, s)
#define string_builder_cwstr(a, b, s) string_builder_cwstr32(a, b, s)
#endif

#define string_builder(arena, builder, x) \
	_Generic((x),                           \
			WString: string_builder_wstr,       \
			wchar: string_builder_wchar,        \
			wchar *: string_builder_cwstr,      \
			String: string_builder_str,         \
			char: string_builder_char,          \
			char *: string_builder_cstr,        \
			default: string_builder_cstr)(arena, builder, x)

#define s_cpy(arena, x)   \
	_Generic((x),           \
			WString: wstr_cpy,  \
			wchar: wchar_cpy,   \
			wchar *: cwstr_cpy, \
			String: str_cpy,    \
			char: char_cpy,     \
			char *: cstr_cpy,   \
			default: cstr_cpy)(arena, x)

#define S(x)                      \
	_Generic((x),                   \
			wchar *: make_string_cwstr, \
			char *: make_string_cstr,   \
			default: make_string_cstr)(x)

String string_from_to(String in, U64 start, U64 end);

WString make_string_cwstr(const wchar *s);
String make_string_cstr(const char *s);

void string_builder_wstr16(Arena *arena, StringBuilder *builder, WString ws);
void string_builder_wchar16(Arena *arena, StringBuilder *builder, wchar wc);
void string_builder_cwstr16(Arena *arena, StringBuilder *builder, const wchar *wc);
void string_builder_wstr32(Arena *arena, StringBuilder *builder, WString ws);
void string_builder_wchar32(Arena *arena, StringBuilder *builder, wchar wc);
void string_builder_cwstr32(Arena *arena, StringBuilder *builder, const wchar *wc);
void string_builder_str(Arena *arena, StringBuilder *builder, String s);
void string_builder_char(Arena *arena, StringBuilder *builder, char c);
void string_builder_cstr(Arena *arena, StringBuilder *builder, const char *c);

String get_string(StringBuilder *builder);

String cstr_cpy(Arena *arena, const char *c);
String cwstr_cpy(Arena *arena, const wchar *c);