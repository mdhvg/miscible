#pragma once
#include "base/base_core.h"
#include "base/arena.h"

#define _DynamicArrayHeader_ \
    struct                   \
    {                        \
        U64 size;            \
        U64 capacity;        \
    }

#define DynamicArray(T, name) \
    struct                    \
    {                         \
        _DynamicArrayHeader_; \
        T *v;                 \
    } name

#define DynamicArray_t(T)     \
    struct                    \
    {                         \
        _DynamicArrayHeader_; \
        T *v;                 \
    }

struct ArrayHeader
{
    S64 size;
    U64 used;
    U64 capacity;
};

#define da_getheader(arr) ((arr) ? ((ArrayHeader *)(arr) - 1) : (0))
#define da_getsize(arr)   ((arr) ? (((ArrayHeader *)(arr) - 1)->size) : (0))
#define da_getcap(arr)    ((arr) ? (((ArrayHeader *)(arr) - 1)->size) : (0))
#define da_clear(arr)     da_setsize(arr, 0)

#define da_setsize(arr, sz)                            \
    do                                                 \
    {                                                  \
        if (arr)                                       \
        {                                              \
            ArrayHeader *h = (ArrayHeader *)(arr) - 1; \
            h->size        = sz;                       \
            h->used        = sizeof(arr[0]) * sz;      \
        }                                              \
    } while (0)
#define _da_setcap(arena, arr, cap, T)                                                        \
    do                                                                                        \
    {                                                                                         \
        if (!*arr)                                                                            \
        {                                                                                     \
            void *base     = arena_push(arena, sizeof(ArrayHeader) + (cap) * sizeof(T));      \
            ArrayHeader *h = (ArrayHeader *)base;                                             \
            h->capacity    = (cap) * sizeof(T);                                               \
            h->size        = 0;                                                               \
            h->used        = 0;                                                               \
            *arr           = (T *)(h + 1);                                                    \
        }                                                                                     \
        else                                                                                  \
        {                                                                                     \
            ArrayHeader *h = (ArrayHeader *)(*arr) - 1;                                       \
            if (h->capacity < (cap))                                                          \
            {                                                                                 \
                void *base      = arena_push(arena, sizeof(ArrayHeader) + (cap) * sizeof(T)); \
                ArrayHeader *h2 = (ArrayHeader *)base;                                        \
                MemoryCopy(h2 + 1, *arr, h->used);                                            \
                h2->capacity = (cap) * sizeof(T);                                             \
                h2->size     = h->size;                                                       \
                h2->used     = h->used;                                                       \
                *arr         = (T *)(h2 + 1);                                                 \
            }                                                                                 \
        }                                                                                     \
    } while (0)
#define da_setcap(arena, arr, capacity, T) _da_setcap(arena, &arr, capacity, T)

#define _da_push(arena, arr, val, T)                                                 \
    do                                                                               \
    {                                                                                \
        if (!*arr)                                                                   \
        {                                                                            \
            void *base      = arena_push(arena, os_info.page_size);                  \
            ArrayHeader *h0 = (ArrayHeader *)base;                                   \
            h0->capacity    = os_info.page_size - sizeof(ArrayHeader);               \
            h0->size        = 0;                                                     \
            h0->used        = 0;                                                     \
            *arr            = (T *)(h0 + 1);                                         \
        }                                                                            \
        else                                                                         \
        {                                                                            \
            ArrayHeader *h0 = (ArrayHeader *)(*arr) - 1;                             \
            U64 req         = h0->used + sizeof(val);                                \
            if (h0->capacity < req)                                                  \
            {                                                                        \
                U64 capacity = h0->capacity * 2;                                     \
                if (capacity < req)                                                  \
                    capacity = req;                                                  \
                void *base      = arena_push(arena, sizeof(ArrayHeader) + capacity); \
                ArrayHeader *h1 = (ArrayHeader *)base;                               \
                T *dest         = (T *)(h1 + 1);                                     \
                MemoryCopy((U8 *)dest, (U8 *)(*arr), h0->used);                      \
                h1->capacity = capacity;                                             \
                h1->size     = h0->size;                                             \
                h1->used     = h0->used;                                             \
                *arr         = dest;                                                 \
            }                                                                        \
        }                                                                            \
                                                                                     \
        ArrayHeader *h    = (ArrayHeader *)(*arr) - 1;                               \
        (*arr)[h->size++] = val;                                                     \
        h->used += sizeof(val);                                                      \
    } while (0)
#define da_push(arena, arr, val, T) _da_push(arena, &arr, val, T)

void *dyn_array_grow(Arena *arena, ArrayHeader *header, void *array, U64 item_size, U64 size, B32 clear_to_zero);

#define dyn_array_item_size(a)  (sizeof(&(a).v))
#define dyn_array_get_header(a) ((ArrayHeader *)&(a))
#define dyn_array_init(a, c, T) {0, c, push_array(a, c, T)}
#define dyn_array_at(a, i)      ((a).v + (i))
#define dyn_array_clear(a)      ((a).size = 0)
#define dyn_array_push(arena, a, value)                                                                                  \
    (*((void **)&(a).v) = dyn_array_grow((arena), dyn_array_get_header((a)), (a).v, dyn_array_item_size((a)), 1, false), \
     (a).v[(a).size++]  = (value))

#define StaticArray(T, name) \
    struct                   \
    {                        \
        T *v;                \
        U64 size;            \
        U64 max;             \
    } name
