// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include "base/arena.h"
#include "base/base_core.h"

#define RingBufferHeader() \
    U64 head;              \
    U64 tail;              \
    U64 capacity

#define RingBuffer_t(T)     \
    struct                  \
    {                       \
        RingBufferHeader(); \
        T *v;               \
    }

#if LANG_CPP
template <typename T>
T _rb_extract_type(T *v);
#define _rb_typeof(rb) decltype(_rb_extract_type((rb).v))
#else
#define _rb_typeof(rb) __typeof__(rb.v[0])
#endif

#define rb_size(rb)    ((rb).tail - (rb).head)
#define rb_isfull(rb)  (rb_size(rb) == (rb).capacity)
#define rb_isempty(rb) ((rb).tail == (rb).head)

#define rb_init(arena, rb, cap) (rb.capacity = NextPow2(cap), ((rb).v = push_array0(arena, (rb).capacity, _rb_typeof(rb))))
#define rb_push(rb, x)          (rb_isfull(rb) ? (0) : (((rb.v[IndexWrapPow2((rb).tail, (rb).capacity)] = (x)), ((rb).tail++)), 1))
#define rb_top(rb, top)         (rb_isempty(rb) ? (0) : (((*top) = (rb).v[IndexWrapPow2((rb).head, (rb).capacity)]), 1))
#define rb_pop(rb)              (rb_isempty(rb) ? (0) : (((rb).head++), 1))
