#pragma once

#include "base/base_core.h"

struct Arena
{
	U8 *base;
	U64 used;
	U64 capacity;
	Arena *next = NULL;
};

struct ArenaArray
{
	U64 size;
	Arena **v;
};

struct Temp
{
	Arena *arena;
	U64 pos;
};

struct ArenaDoubleBuffer
{
	Arena *buffer[2];
	Arena *front;
	Arena *back;
	U8 active;
};

Arena *arena_alloc(U64 capacity);
void *arena_push(Arena *a, U64 size, U64 align = 8);
void *arena_realloc(Arena *a, void *ptr, U64 old_size, U64 new_size);
#define realloc_array(a, ptr, count, new_count, type) (type *)arena_realloc(a, ptr, count * sizeof(type), new_count * sizeof(type))
#define push_array(a, count, type)					  (type *)arena_push(a, (count) * sizeof(type))
#define push_struct(a, type)						  (type *)arena_push(a, sizeof(type))
#define push_size(a, size, type)					  (type *)arena_push(a, size, 8)

inline U64 arena_get(Arena *a)
{
	return a->used;
}
inline void arena_pop(Arena *a, U64 pos)
{
	a->used = pos;
}

void arena_clear(Arena *a);
void arena_free(Arena *a);

Temp temp_begin(Arena *arena);
void temp_end(Temp temp);

ArenaArray arena_array_alloc(U64 capacity, U64 size);
void arena_array_free(ArenaArray aa);
void arena_array_clear(ArenaArray aa);

void arena_db_alloc(ArenaDoubleBuffer *arena, U64 capacity);
void arena_db_switch(ArenaDoubleBuffer *arena);