#pragma once
#include "os/os_inc.h"
#include "base/arena.h"
#include "base/base_core.h"

struct ArrayHeader
{
    S64 size;
    U64 capacity;
};

#define getheader(arr) ((arr) ? ((ArrayHeader *)(arr) - 1) : (0))

/*******************************************************************************
* Dynamic array (DA)
*******************************************************************************/
#define da_getsize(arr) ((arr) ? (getheader(arr)->size) : (0))
#define da_getcap(arr)  ((arr) ? (getheader(arr)->capacity) : (0))
#define da_clear(arr)   da_setsize((arr), 0)
#define da_setsize(arr, sz)                  \
    do                                       \
    {                                        \
        if (arr)                             \
        {                                    \
            ArrayHeader *h = getheader(arr); \
            h->size = sz;                    \
        }                                    \
    } while (0)

template <typename T>
void _da_setcap(Arena *arena, T **arr, U64 cap)
{
    if (!*arr)
    {
        void *base = arena_push(arena, sizeof(ArrayHeader) + (cap) * sizeof(T));
        ArrayHeader *h = (ArrayHeader *)base;
        h->capacity = cap;
        h->size = 0;
        *arr = (T *)(h + 1);
    }
    else
    {
        ArrayHeader *h = getheader(*arr);
        if (h->capacity < (cap))
        {
            void *base = arena_push(arena, sizeof(ArrayHeader) + (cap) * sizeof(T));
            ArrayHeader *h2 = (ArrayHeader *)base;
            MemoryCopy(h2 + 1, *arr, h->size * sizeof(T));
            h2->size = h->size;
            h2->capacity = cap;
            *arr = (T *)(h2 + 1);
        }
    }
}
#define da_setcap(arena, arr, capacity) _da_setcap(arena, &(arr), capacity)

template <typename T>
void _da_push(Arena *arena, T **arr, T val)
{
    if (!*arr)
    {
        void *base = arena_push(arena, os_info.page_size);
        ArrayHeader *h0 = (ArrayHeader *)base;
        h0->capacity = (os_info.page_size - sizeof(ArrayHeader)) / sizeof(T);
        h0->size = 0;
        *arr = (T *)(h0 + 1);
    }
    else
    {
        ArrayHeader *h0 = getheader(*arr);
        U64 req = h0->size + 1;
        if (h0->capacity < req)
        {
            U64 capacity = h0->capacity * 2;
            if (capacity < req)
                capacity = req;
            void *base = arena_push(arena, sizeof(ArrayHeader) + capacity * sizeof(T));
            ArrayHeader *h1 = (ArrayHeader *)base;
            T *dest = (T *)(h1 + 1);
            MemoryCopy((U8 *)dest, (U8 *)(*arr), h0->size * sizeof(T));
            h1->capacity = capacity;
            h1->size = h0->size;
            *arr = dest;
        }
    }

    ArrayHeader *h = getheader(*arr);
    (*arr)[h->size++] = val;
}
#define da_push(arena, arr, val) (_da_push(arena, &arr, val), da_getsize(arr) - 1)

/*******************************************************************************
* Virtual memory array (VA)
*******************************************************************************/
#define va_free(arr) ((arr)                                                     \
                          ? (os_release(                                        \
                                getheader(arr),                                 \
                                sizeof(ArrayHeader) +                           \
                                    sizeof(arr[0]) * getheader(arr)->capacity)) \
                          : (0))
#define va_getcap(arr)  ((arr) ? (getheader(arr)->capacity) : (0))
#define va_getsize(arr) ((arr) ? (getheader(arr)->size) : (0))

template <typename T>
void _va_push(T **arr, T val)
{
    if (!*arr)
    {
        void *base = os_reserve(NULL, MB(10));
        os_commit(base, os_info.page_size); // Commit 1 page
        ArrayHeader *h = (ArrayHeader *)base;
        h->capacity = (os_info.page_size - sizeof(ArrayHeader)) / sizeof(T);
        h->size = 0;
        *arr = (T *)(h + 1);
    }
    else
    {
        ArrayHeader *h0 = getheader(*arr);
        U64 req = (h0->size + 1) * sizeof(T);
        if (h0->capacity * sizeof(T) < req)
        {
            U64 capacity = h0->capacity * 2;
            if (capacity * sizeof(T) < req)
                capacity = req;
            os_commit(h0, AlignOf(sizeof(ArrayHeader) + capacity * sizeof(T), os_info.page_size));
            h0->capacity = capacity;
        }
    }

    ArrayHeader *h = getheader(*arr);
    (*arr)[h->size++] = val;
}
#define va_push(arr, val) (_va_push(&arr, val), va_getsize(arr) - 1)
