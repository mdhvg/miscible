#include "base/arena.h"
#include "arena.h"

Arena *head = NULL;

Arena *arena_alloc(U64 capacity)
{
	Arena *a	= (Arena *)malloc(sizeof(Arena) + capacity);
	a->base		= (U8 *)(a + 1);
	a->used		= 0;
	a->capacity = capacity;
	a->next		= NULL;
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

void *arena_push(Arena *a, U64 size, U64 align)
{
	if (!size) return NULL;
	Assert(a);
	U64 start = (U64)a->base + a->used + AlignOf(align);
	U64 end	  = start + size;
	if (end > (U64)a->base + a->capacity)
	{
		printf("[Arena Out of memory]: Capacity: %zu, Need: %zu (%zu more)\n", a->capacity, size, end - ((U64)a->base + a->capacity));
		Assert(0);
	}
	void *result = (void *)start;
	a->used		 = end - (U64)a->base;
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

void arena_clear(Arena *a)
{
	a->used = 0;
}
void arena_free(Arena *a)
{
	free(a);
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
	aa.v		  = push_array(pics.persistent_arena, size, Arena *);
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
	if (arena->active)
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
