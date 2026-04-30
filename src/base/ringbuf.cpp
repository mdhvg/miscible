#include "base/ringbuf.h"

B32 _rb_push(RingBufferHeader *rb, S64 max_size, U64 t_size, void *data)
{
    if (rb->size >= max_size)
        return 0;
    S64 write_idx = rb->tail++;
    U8 *write_ptr = (U8 *)(rb + 1) + (t_size * write_idx);
    MemoryCopy(write_ptr, data, t_size);
    rb->tail %= max_size;
    rb->size++;
    return 1;
}

S64 _rb_pop(RingBufferHeader *rb, S64 max_size)
{
    rb->size--;
    ++rb->head %= max_size;
    return (rb->head - 1) % max_size;
}
