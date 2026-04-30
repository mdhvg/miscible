#pragma once
#define DoubleBuffer(T, name) \
    struct                    \
    {                         \
        T a;                  \
        T b;                  \
        B32 cur;              \
    } name;
#define DBuf_switch(x) (((x).cur == 0) ? (x).cur = 1 : (x).cur = 0)
#define DBuf_get(x)    (((x).cur == 0) ? (x).a : (x).b)
#define DBuf_back(x)   (((x).cur == 0) ? (x).b : (x).a)
