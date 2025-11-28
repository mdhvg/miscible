#include "base/array.h"

void *array_grow(Arena *arena, ArrayHeader *header, void *array, U64 item_size, U64 grow_by, B32 clear_to_zero)
{
	if (header->capacity < header->count + grow_by)
	{
		U64 new_size = header->capacity * 2;
		if (new_size < header->capacity + grow_by) new_size = header->capacity + grow_by;
		array = arena_realloc(arena, array, header->count * item_size, new_size * item_size);
	}
	return array;
}