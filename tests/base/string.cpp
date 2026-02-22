#include "doctest.h"

#include "base/arena.h"
#include "base/string.h"

extern Arena *test_arena;

TEST_CASE("String empty")
{
    StringBuilder s = string_empty(test_arena);
    CHECK(s.size == 0);
    CHECK(strcmp(CStrCast(s), "") == 0);
}

TEST_CASE("make_string_cwstr basic")
{
    WString ws = string_cpy(test_arena, L"wide");
    CHECK(ws.size == 4);
}

TEST_CASE("string_cpy char* deep copy")
{
    const char *src = "copytest";
    String s1       = string_cpy(test_arena, src);
    CHECK(s1.size == 8);
    CHECK(strcmp(CStrCast(s1), "copytest") == 0);
}

TEST_CASE("string_cpy unsigned char*")
{
    const unsigned char *src = (const unsigned char *)"abc";
    String s                 = string_cpy(test_arena, src);
    CHECK(strcmp(CStrCast(s), "abc") == 0);
}

TEST_CASE("string_cmp exact match")
{
    String s = string_cpy(test_arena, "hello");
    CHECK_FALSE(string_cmp(s, "hello"));
}

TEST_CASE("string_cmp mismatch")
{
    String s = string_cpy(test_arena, "hello");
    CHECK(string_cmp(s, "world") < 0);
}

TEST_CASE("string_cmp limit shorter than string")
{
    String s = string_cpy(test_arena, "abcdef");
    CHECK_FALSE(string_cmp(s, "abc", 3));
    CHECK_FALSE(string_cmp(s, "abc", 6));
}

TEST_CASE("string_push char")
{
    StringBuilder sb = string_empty(test_arena, 2);
    string_push(&sb, 'X');
    CHECK(sb.size == 1);
    CHECK(sb.v[0] == 'X');
}

TEST_CASE("string_push cstr")
{
    StringBuilder sb = string_empty(test_arena, 2);
    string_push(&sb, "hi");
    CHECK(sb.size == 2);
    CHECK(strncmp((char *)sb.v, "hi", 2) == 0);
}

TEST_CASE("string_push grow capacity")
{
    StringBuilder sb = string_empty(test_arena, 1);
    string_push(&sb, "abc");
    CHECK(sb.size == 3);
    CHECK(strncmp((char *)sb.v, "abc", 3) == 0);
}

TEST_CASE("string_from_to valid range")
{
    String s   = string_cpy(test_arena, "abcdef");
    String sub = string_from_to(s, 2, 4);
    CHECK(sub.size == 2);
    CHECK(strncmp(CStrCast(sub), "cd", 2) == 0);
}

TEST_CASE("string_from_to start==end")
{
    String str = sv("abcdef");
    String sub = string_from_to(str, 3, 3);
    CHECK(sub.size == 0);
}

TEST_CASE("string_format basic")
{
    StringBuilder base = string_empty(test_arena, 256);
    string_format(&base, "num=%d str=%s", 42, "ok");
    CHECK(strcmp(CStrCast(base), "num=42 str=ok") == 0);
}

// TEST_CASE("string_formatw basic")
// {
//     WString ws = string_formatw(test_arena, L"wide %d", 7);
//     CHECK(ws.size > 0);
// }

TEST_CASE("match_front char*")
{
    String a = sv("abcdef");
    CHECK(match_front(a, "abc"));
    CHECK_FALSE(match_front(a, "def"));
}

TEST_CASE("match_end char*")
{
    String a = sv("abcdef");
    CHECK(match_end(a, "def"));
    CHECK_FALSE(match_end(a, "abc"));
    CHECK_FALSE(match_end(a, "0abcdef"));
}

TEST_CASE("match_front wchar*")
{
    WString a = sv(L"abcdef");
    CHECK(match_front(a, L"abc"));
}

TEST_CASE("match_end wchar*")
{
    WString a = sv(L"abcdef");
    CHECK(match_end(a, L"def"));
}

TEST_CASE("string_cmp with limit")
{
    String s = string_cpy(test_arena, "abcdef");
    CHECK(string_cmp(s, "abc", 3) == 0);
    CHECK(string_cmp(s, "abc", 6) == 0);
}

TEST_CASE("string_push char and grow")
{
    StringBuilder sb = string_empty(test_arena, 2);
    string_push(&sb, 'A');
    string_push(&sb, 'B');
    string_push(&sb, 'C');
    CHECK(sb.size == 3);
    CHECK(sb.v[0] == 'A');
    CHECK(sb.v[2] == 'C');
}

TEST_CASE("string write and grow")
{
    StringBuilder sb = string_empty(test_arena, 2);
    string_push(&sb, "ab");
    CHECK(sb.capacity == 4);
    string_growto(&sb, 10);
    CHECK(sb.capacity == 10);
    string_push(&sb, "beb");
    string_growby(&sb, 4);
    CHECK(sb.capacity == 10);
}

TEST_CASE("string_format simple")
{
    StringBuilder base = string_empty(test_arena, 256);
    string_format(&base, "Hello %s %d", "world", 42);
    CHECK(strcmp(CStrCast(base), "Hello world 42") == 0);
}

TEST_CASE("string_replace")
{
    StringBuilder base = string_empty(test_arena, 256);
    string_push(&base, "Hello,-,How,-,Are,-,,-,You,-,");

    string_replace(&base, ",-,", "*-*");
    CHECK(base.size == 29);
    CHECK_FALSE(string_cmp(StringCast(base), "Hello*-*How*-*Are*-**-*You*-*"));

    string_replace(&base, "*-*", " ");
    CHECK(base.size == 19);
    CHECK_FALSE(string_cmp(StringCast(base), "Hello How Are  You "));

    string_replace(&base, " ", ",-,");
    CHECK(base.size == 29);
    CHECK_FALSE(string_cmp(StringCast(base), "Hello,-,How,-,Are,-,,-,You,-,"));

    string_clear(base);
    string_push(&base, "Hello,-,How,-,Are,-,You");

    string_replace(&base, ",-,", " ");
    CHECK(base.size == 17);
    CHECK_FALSE(string_cmp(StringCast(base), "Hello How Are You"));

    string_replace(&base, " ", ",-,");
    CHECK(base.size == 23);
    CHECK_FALSE(string_cmp(StringCast(base), "Hello,-,How,-,Are,-,You"));
}

TEST_CASE("match_end and match_front")
{
    String a = sv("hello");
    CHECK(match_front(a, "he"));
    CHECK(match_end(a, "lo"));
    CHECK_FALSE(match_front(a, "lo"));
    CHECK_FALSE(match_end(a, "he"));
}
