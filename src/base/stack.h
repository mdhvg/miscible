#pragma once

#include "base/base_core.h"

#define stack_def(name, t, s) \
	struct                    \
	{                         \
		t v[s];               \
		U64 size;             \
	} name = {0}

#define stack_push(s, x) ((s).v[(s).size++] = (x))
#define stack_pop(s)	 ((s).v[--(s).size])
#define stack_front(s)	 ((s).v + (s).size - 1)
#define stack_full(s)	 (sizeof((s).v) <= (s).size)
#define stack_empty(s)	 ((s).size == 0)