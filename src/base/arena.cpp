#include "os/os_inc.h"
#include "base/arena.h"
#include "base/log.h"
#include "arena.h"
#include "base_core.h"

Arena *arena_head = NULL;

#if ARENA_DBG
void _arena_alloc(U64 capacity, Arena **arena, const char *name)
#else
void _arena_alloc(U64 capacity, Arena **arena)
#endif
{
    void *base = os_reserve(NULL, capacity);
    Assert(base, "alloc failed");
    U64 cmt_size = os_info.page_size;
    os_commit(base, cmt_size);

    Arena *a = (Arena *)base;
    a->base = (U8 *)(a + 1);
    a->used = 0;
    a->min_use = 0;
    a->capacity = capacity;
    a->min_cmt = os_info.page_size;
    a->cmt_size = cmt_size;

#if ARENA_DBG
    a->next = NULL;
    a->name = name;

    if (arena_head == NULL)
    {
        arena_head = a;
    }
    else
    {
        Arena *cur = arena_head;
        while (cur->next)
            cur = cur->next;
        cur->next = a;
    }
#endif

    *arena = a;
}

void *arena_push(Arena *a, U64 size, U8 zero, U64 align)
{
    if (size == 0)
        return NULL;
    Assert(a, "arena not allocated");

    align = MAX(align, 8);
    U64 start = AlignOf(((U64)a->base + a->used), align);
    U64 end = start + size;
    U64 need_size = end - ((U64)a->base + a->used);
    Assert(end <= a->capacity + (U64)a->base, "(Out of memory) Used: %zu/%zu, Need: %zu (%zu more)\n", a->used, a->capacity, need_size, need_size - a->capacity);

    // NOTE: cmt_size is arena aligned NOT base aligned
    U64 commit_size = end - (U64)a;
    commit_size = AlignOf(commit_size, os_info.page_size);

    if (commit_size > a->cmt_size)
    {
        // Then we need to commit more memory
        // mscbl_log_dbg(Arena, "Commiting memory: 0x%X\tsize: %zu", a->base, cmt_size);
        os_commit(a, commit_size);
    }

    void *result = (void *)(start);

    a->used += need_size;
    a->cmt_size = commit_size;

    if (zero)
        MemoryZero(result, size);
    return result;
}

void *arena_realloc(Arena *a, void *ptr, U64 old_size, U64 new_size)
{
    if (new_size <= old_size) return ptr;
    Assert(a, "arena is NULL");
    void *new_ptr = arena_push(a, new_size);
    MemoryCopy(new_ptr, ptr, old_size);
    return new_ptr;
}

void arena_pop(Arena *a, U64 pos)
{
    Assert(a, "arena is NULL");
    U64 decmt_pos = MAX(a->min_cmt, AlignOf((U64)a->base + pos, os_info.page_size));
    U64 decmt_size = a->cmt_size - (decmt_pos - (U64)a);

    os_decommit((void *)decmt_pos, decmt_size);
    a->cmt_size -= decmt_size;
    a->used = MAX(a->min_use, pos);
}

void arena_free(Arena *a)
{
#if ARENA_DBG
    Arena *parent = arena_head;
    while (parent->next != a)
        parent = parent->next;
    parent->next = a->next;
#endif
    os_release(a, a->cmt_size);
}

Temp temp_begin(Arena *arena)
{
    U64 pos = arena_get(arena);
    Temp t = {arena, pos};
    return t;
}

void temp_end(Temp temp)
{
    if (temp.arena)
        arena_pop(temp.arena, temp.pos);
}

ArenaArray arena_array_alloc(U64 capacity, U64 size)
{
    Arena *first = NULL;
    arena_alloc(capacity, first);

    ArenaArray aa = {0};
    aa.size = size;
    aa.v = push_array(first, size, Arena *);

    arena_setmin(first, arena_get(first));

    aa.v[0] = first;
    for (U64 i = 1; i < size; i++)
    {
        arena_alloc(capacity, aa.v[i]);
    }
    return aa;
}

void arena_array_free(ArenaArray aa)
{
    for (U64 i = aa.size - 1; i >= 1; i--)
    {
        arena_free(aa.v[i]);
        aa.v[i] = NULL;
    }
    arena_free(aa.v[0]);
}

void arena_array_clear(ArenaArray aa)
{
    for (U64 i = 0; i < aa.size; i++)
    {
        arena_clear(aa.v[i]);
    }
}

// void arena_db_alloc(ArenaDoubleBuffer *arena, U64 capacity)
// {
//     arena_alloc(capacity, arena->buffer[0]);
//     arena_alloc(capacity, arena->buffer[1]);
//     arena->front  = arena->buffer[0];
//     arena->back   = arena->buffer[1];
//     arena->active = 0;
// }
//
// void arena_db_switch(ArenaDoubleBuffer *arena)
// {
//     if (arena->active == 1)
//     {
//         arena->front  = arena->buffer[0];
//         arena->back   = arena->buffer[1];
//         arena->active = 0;
//     }
//     else
//     {
//         arena->front  = arena->buffer[1];
//         arena->back   = arena->buffer[0];
//         arena->active = 1;
//     }
//     arena_clear(arena->front);
// }
