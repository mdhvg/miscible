#pragma once

#include "base/base_core.h"
#include "base/arena.h"

#define _DynamicArrayHeader_ \
	struct                   \
	{                        \
		U64 size;            \
		U64 capacity;        \
	}

#define DynamicArray(t, name) \
	struct                    \
	{                         \
		_DynamicArrayHeader_; \
		t *v;                 \
	} name

#define DynamicArray_t(t)     \
	struct                    \
	{                         \
		_DynamicArrayHeader_; \
		t *v;                 \
	}

typedef _DynamicArrayHeader_ ArrayHeader;

void *dyn_array_grow(Arena *arena, ArrayHeader *header, void *array, U64 item_size, U64 size, B32 clear_to_zero);

#define dyn_array_item_size(a)	(sizeof(&(a).v))
#define dyn_array_get_header(a) ((ArrayHeader *)&(a))
#define dyn_array_init(a, c, t) {0, c, push_array(a, c, t)}
#define dyn_array_at(a, i)		((a).v[(i)])
#define dyn_array_push(arena, a, value)                                                                                  \
	(*((void **)&(a).v) = dyn_array_grow((arena), dyn_array_get_header((a)), (a).v, dyn_array_item_size((a)), 1, false), \
	 (a).v[(a).size++]	= (value))

#define StaticArray(t, name) \
	struct                   \
	{                        \
		t *v;                \
		U64 size;            \
		U64 max;             \
	} name
