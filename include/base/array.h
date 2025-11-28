#include "base/core.h"

#define _ArrayHeader_ \
	struct              \
	{                   \
		U64 count;        \
		U64 capacity;     \
	}

typedef _ArrayHeader_ ArrayHeader;

void *array_grow(Arena *arena, ArrayHeader *header, void *array, U64 item_size, U64 count, B32 clear_to_zero);

#define array_item_size(a)	(sizeof(&(a).v))
#define array_get_header(a) ((ArrayHeader *)&(a))

#define array_push(arena, a, val)                                                                          \
	(*((void **)&(a).v) = array_grow((arena), array_get_header((a)), (a).v, array_item_size((a)), 1, false), \
	 (a).v[(a).count++] = (value))