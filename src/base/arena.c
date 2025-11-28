#include "base/arena.h"
#include "Debug.h"

Arena *persistent_arena;
Arena *head = NULL;

Arena *arena_alloc(U64 capacity)
{
	Arena *a = (Arena *)malloc(sizeof(Arena) + capacity);
	a->base = (U8 *)(a + 1);
	a->used = 0;
	a->capacity = capacity;
	a->next = NULL;
	if (persistent_arena == NULL)
	{
		persistent_arena = a;
	}
	if (head == NULL)
	{
		head = a;
	}
	else
	{
		Arena *cur = head;
		while (cur->next)
		{
			cur = cur->next;
		}
		cur->next = a;
	}
	return a;
}

void *arena_push(Arena *a, U64 size, U64 align)
{
	ASSERT(a);
	U64 start = (U64)a->base + a->used + AlignOf(align);
	U64 end = start + size;
	ASSERT(end <= (U64)a->base + a->capacity);
	void *result = (void *)start;
	a->used = end - (U64)a->base;
	return result;
}

void *arena_realloc(Arena *a, void *ptr, U64 old_size, U64 new_size)
{
	ASSERT(a);
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