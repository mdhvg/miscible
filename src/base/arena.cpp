#include "os/os_inc.h"
#include "base/arena.h"
#include "base/log.h"
#include "arena.h"
#include "base_core.h"

Arena *head = NULL;

Arena *arena_alloc(U64 capacity)
{
	void *base	 = os_reserve(NULL, capacity);
	U64 cmt_size = os_info.page_size;
	Assert(base && os_commit(base, cmt_size));

	Arena *a	= (Arena *)base;
	a->base		= (U8 *)(a + 1);
	a->used		= 0;
	a->capacity = capacity;
	a->cmt_size = cmt_size;

	// TODO: Arena chain for debug/memory usage view
	a->next = NULL;

	// if (persistent_arena == NULL)
	// {
	// 	persistent_arena = a;
	// }
	// if (head == NULL)
	// {
	// 	head = a;
	// }
	// else
	// {
	// 	Arena *cur = head;
	// 	while (cur->next)
	// 	{
	// 		cur = cur->next;
	// 	}
	// 	cur->next = a;
	// }

	return a;
}

void *arena_push(Arena *a, U64 size, U64 align, U8 zero)
{
	if (size == 0)
		return NULL;
	Assert(a);

	align		 = MAX(align, 8);
	U64 start	 = AlignOf(((U64)a->base + a->used), align);
	U64 end		 = start + size;
	U64 req_size = end - (U64)a->base;
	U64 cmt_size = AlignOf(req_size, os_info.page_size);

	if (req_size > a->capacity)
	{
		msc_log_error(Arena, "(Out of memory) Used: %zu/%zu, Need: %zu (%zu more)\n", a->used, a->capacity, req_size, req_size - a->capacity);
		Assert(0);
	}

	if (req_size > a->cmt_size)
	{
		// Then we need to commit more memory
		os_commit(a->base, cmt_size);
	}

	void *result = (void *)(a->base + a->used);

	a->used		= req_size;
	a->cmt_size = cmt_size;

	if (zero) MemoryZero(result, req_size);
	return result;
}

void *arena_realloc(Arena *a, void *ptr, U64 old_size, U64 new_size)
{
	if (new_size <= old_size) return ptr;
	Assert(a);
	void *new_ptr = arena_push(a, new_size);
	MemoryCopy(new_ptr, ptr, old_size);
	return new_ptr;
}

void arena_pop(Arena *a, U64 pos)
{
	U64 decmt_pos  = AlignOf((U64)a->base + pos, os_info.page_size);
	U64 decmt_size = a->cmt_size - (decmt_pos - (U64)a);

	os_decommit((void *)decmt_pos, decmt_size);
	a->cmt_size -= decmt_size;
	a->used = pos;
}

void arena_free(Arena *a)
{
	os_release(a);
}

Temp temp_begin(Arena *arena)
{
	U64 pos = arena_get(arena);
	Temp t	= {arena, pos};
	return t;
}

void temp_end(Temp temp)
{
	arena_pop(temp.arena, temp.pos);
}

ArenaArray arena_array_alloc(U64 capacity, U64 size)
{
	ArenaArray aa = {0};
	aa.size		  = size;
	aa.v		  = push_array(mscbl.persistent_arena, size, Arena *);
	for (U64 i = 0; i < size; i++)
	{
		aa.v[i] = arena_alloc(capacity);
	}
	return aa;
}

void arena_array_free(ArenaArray aa)
{
	for (U64 i = 0; i < aa.size; i++)
	{
		arena_free(aa.v[i]);
		aa.v[i] = NULL;
	}
}

void arena_array_clear(ArenaArray aa)
{
	for (U64 i = 0; i < aa.size; i++)
	{
		arena_clear(aa.v[i]);
	}
}

void arena_db_alloc(ArenaDoubleBuffer *arena, U64 capacity)
{
	arena->buffer[0] = arena_alloc(capacity);
	arena->buffer[1] = arena_alloc(capacity);
	arena->front	 = arena->buffer[0];
	arena->back		 = arena->buffer[1];
	arena->active	 = 0;
}

void arena_db_switch(ArenaDoubleBuffer *arena)
{
	if (arena->active == 1)
	{
		arena->front  = arena->buffer[0];
		arena->back	  = arena->buffer[1];
		arena->active = 0;
	}
	else
	{
		arena->front  = arena->buffer[1];
		arena->back	  = arena->buffer[0];
		arena->active = 1;
	}
	arena_clear(arena->front);
}
