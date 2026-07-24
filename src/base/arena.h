// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include "base/base_core.h"

typedef struct Arena Arena;
struct Arena
{
    U8 *base;
    U64 used;
    U64 min_use;
    U64 cmt_size;
    U64 min_cmt;
    U64 capacity;
#if DBG
    Arena *next;
    const char *name;
#endif
};

#if DBG
MSCBL_API Arena *arena_head;
#endif

typedef struct ArenaArray ArenaArray;
struct ArenaArray
{
    U64 size;
    Arena **v;
};

typedef struct Temp Temp;
struct Temp
{
    Arena *arena;
    U64 pos;
};

#if DBG
MSCBL_API void _arena_alloc(U64 capacity, Arena **arena, const char *name);
#define arena_alloc(capacity, arena) _arena_alloc(capacity, &(arena), Stringify(arena))
#else
MSCBL_API void _arena_alloc(U64 capacity, Arena **arena);
#define arena_alloc(capacity, arena) _arena_alloc(capacity, &(arena))
#endif

struct _arena_push_args
{
    Arena *arena;
    U64 size;
    U64 align;
    U8 zero;
};
MSCBL_API void *_arena_push(struct _arena_push_args args);
#if LANG_CPP
#define arena_push(...) _arena_push(_arena_push_args{__VA_ARGS__})
#else
#define arena_push(...) _arena_push((struct _arena_push_args){__VA_ARGS__})
#endif
MSCBL_API void *_arena_resize(Arena *a, void *old_ptr, U64 old_size, U64 new_size);
#define arena_resize(a, op, os, ns) _arena_resize(a, op, os, ns)
#define push_array(a, count, type)  (type *)arena_push(.arena = a, .size = (count) * sizeof(type))
#define push_array0(a, count, type) (type *)arena_push(.arena = a, .size = (count) * sizeof(type), .zero = 1)
#define push_struct(a, type)        (type *)arena_push(.arena = a, .size = sizeof(type))
#define push_struct0(a, type)       (type *)arena_push(.arena = a, .size = sizeof(type), .zero = 1)
#define push_size(a, sz, type)      (type *)arena_push(.arena = a, .size = (U64)sz, .align = 8)

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
