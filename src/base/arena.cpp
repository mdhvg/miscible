#include "os/os_inc.h"
#include "base/arena.h"
#include "base/log.h"
#include "arena.h"
#include "base_core.h"

Arena *arena_head = NULL;

#if DBG
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

#if DBG
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

void *_arena_push(struct _arena_push_args args)
{
    if (args.size == 0)
        return NULL;
    Assert(args.arena, "arena not allocated");

    args.align = MAX(args.align, 8);
    U64 start = AlignOf(((U64)args.arena->base + args.arena->used), args.align);
    U64 end = start + args.size;
    U64 need_size = end - ((U64)args.arena->base + args.arena->used);
    Assert(end <= args.arena->capacity + (U64)args.arena->base, "(Out of memory) Used: %zu/%zu, Need: %zu (%zu more)\n", args.arena->used, args.arena->capacity, need_size, need_size + args.arena->used - args.arena->capacity);

    // NOTE: cmt_size is arena aligned NOT base aligned
    U64 commit_size = end - (U64)args.arena;
    commit_size = AlignOf(commit_size, os_info.page_size);

    if (commit_size > args.arena->cmt_size)
    {
        // Then we need to commit more memory
        // mscbl_log_info(Arena, "Commiting memory: 0x%X\tsize: %zu", args.arena->base, cmt_size);
        os_commit(args.arena, commit_size);
    }

    void *result = (void *)(start);

    args.arena->used += need_size;
    args.arena->cmt_size = commit_size;

    if (args.zero)
        MemoryZero(result, args.size);
    return result;
}

void *_arena_resize(Arena *a, void *old_ptr, U64 old_size, U64 new_size)
{
    if (!new_size)
        return NULL;
    if (!old_ptr || old_size == 0)
        return arena_push(.arena = a, .size = new_size);

    U8 *old_bytes = (U8 *)old_ptr;
    U64 old_offset = (U64)old_bytes + old_size - (U64)a->base;

    if (old_offset == a->used)
    {
        a->used -= old_size;
        void *new_ptr = arena_push(.arena = a, .size = new_size, .align = 1, .zero = 0);

        mscbl_log_info("Saved %zu bytes", old_size);
        Assert(new_ptr == old_ptr, "In-place arena resize pointer mismatch");
        return new_ptr;
    }

    void *new_ptr = arena_push(a, new_size);
    if (new_ptr)
    {
        U64 copy_size = (old_size < new_size) ? old_size : new_size;
        MemoryCopy(new_ptr, old_ptr, copy_size);
    }

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
    Assert(a, "arena is NULL");
#if DBG
    Arena **cur = &arena_head;
    while (*cur && *cur != a)
    {
        cur = &(*cur)->next;
    }

    if (*cur)
    {
        *cur = a->next;
    }
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
