#include "base/array.h"

void *array_grow(Arena *arena, ArrayHeader *header, void *array, U64 item_size, U64 size, B32 clear_to_zero)
{
	if (header->capacity < header->size + size)
	{
		U64 new_size = header->capacity * 2;
		if (new_size < header->capacity + size) new_size = header->capacity + size;
		array = arena_realloc(arena, array, header->size * item_size, new_size * item_size);
	}
	return array;
}