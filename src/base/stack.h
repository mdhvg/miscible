// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#define Stack(name, T) \
    struct             \
    {                  \
        U64 size;      \
        U64 max;       \
        T *v;          \
    } name

#define stack_init(a, c, T) {0, c, push_array(a, c, T)}
#define stack_push(s, x)    ((s).v[(s).size++] = (x))
#define stack_pop(s)        ((s).v[--(s).size])
#define stack_front(s)      ((s).v + (s).size - 1)
#define stack_full(s)       ((s).size >= (s).max)
#define stack_empty(s)      ((s).size == 0)
