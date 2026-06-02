#include "base/ringbuf.h"

S64 _rb_push(RingBufferHeader *rb, S64 max_size)
{
    S64 res = rb->tail;
    rb->size++;
    rb->tail++;
    rb->tail %= max_size;
    return res;
}

S64 _rb_pop(RingBufferHeader *rb, S64 max_size)
{
    S64 res = rb->head;
    rb->size--;
    rb->head++;
    rb->head %= max_size;
    return res;
}
