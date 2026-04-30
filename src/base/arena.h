#pragma once
#include "base/base_core.h"

#define ARENA_DBG 1

struct Arena
{
    U8 *base;
    U64 used;
    U64 min_use;
    U64 cmt_size;
    U64 min_cmt;
    U64 capacity;
#if ARENA_DBG
    Arena *next;
    const char *name;
#endif
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

// struct ArenaDoubleBuffer
// {
//     Arena *buffer[2];
//     Arena *front;
//     Arena *back;
//     U8 active;
// };

MSCBL_API Arena *arena_head;

#if ARENA_DBG
void _arena_alloc(U64 capacity, Arena **arena, const char *name);
#define arena_alloc(capacity, arena) _arena_alloc(capacity, &(arena), Stringify(arena))
#else
void _arena_alloc(U64 capacity, Arena **arena);
#define arena_alloc(capacity, arena) _arena_alloc(capacity, &(arena))
#endif

void *arena_push(Arena *a, U64 size, U8 zero = 0, U64 align = 8);
void *arena_realloc(Arena *a, void *ptr, U64 old_size, U64 new_size);
#define realloc_array(a, ptr, count, new_count, type) (type *)arena_realloc(a, ptr, count * sizeof(type), new_count * sizeof(type))
#define push_array(a, count, type)                    (type *)arena_push(a, (count) * sizeof(type))
#define push_array0(a, count, type)                   (type *)arena_push(a, (count) * sizeof(type), 1)
#define push_struct(a, type)                          (type *)arena_push(a, sizeof(type))
#define push_struct0(a, type)                         (type *)arena_push(a, sizeof(type), 1)
#define push_size(a, size, type)                      (type *)arena_push(a, size, 8)

void arena_pop(Arena *a, U64 pos);
void arena_free(Arena *a);
inline U64 arena_get(Arena *a)
{
    return a->used;
}
inline void arena_clear(Arena *a)
{
    arena_pop(a, a->min_use);
}
inline void arena_setmin(Arena *a, U64 pos)
{
    a->min_use = pos;
}
inline void arena_setcmt(Arena *a, U64 cmt)
{
    a->min_cmt = cmt;
}

MSCBL_API Temp temp_begin(Arena *arena);
MSCBL_API void temp_end(Temp temp);

ArenaArray arena_array_alloc(U64 capacity, U64 size);
void arena_array_free(ArenaArray aa);
void arena_array_clear(ArenaArray aa);

#define check_arena(a) ((!a) && (arena_alloc(Glue(a, _DEFAULT), a)))

// void arena_db_alloc(ArenaDoubleBuffer *arena, U64 capacity);
// void arena_db_switch(ArenaDoubleBuffer *arena);
