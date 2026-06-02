#pragma once
#include "base/base_core.h"

struct RingBufferHeader
{
    S64 size;
    S64 head;
    S64 tail;
};

#define RingBuffer(T, name, sz) \
    struct                      \
    {                           \
        S64 size;               \
        S64 head;               \
        S64 tail;               \
        T v[sz];                \
    } name

// #define _rb_read(rb)     (((++rb.head) %= StaticArrSize(rb.v)), (rb.v[(rb.head - 1) % StaticArrSize(rb.v)]))
// #define _rb_write(rb, x) ((rb.tail %= StaticArrSize(rb.v)))
#define _rb_header(rb) (((RingBufferHeader *)(&(rb))))

#define rb_top(rb)     ((rb).v[rb.head])
#define rb_pop(rb)     ((rb).v[_rb_pop(_rb_header(rb), StaticArrSize((rb).v))])
#define rb_push(rb, x) ((rb).v[_rb_push(_rb_header(rb), StaticArrSize((rb).v))] = (x))
#define rb_getsize(rb) ((rb).size)
#define rb_isfull(rb)  ((rb).size >= StaticArrSize(rb.v))

S64 _rb_push(RingBufferHeader *rb, S64 max_size);
S64 _rb_pop(RingBufferHeader *rb, S64 max_size);
