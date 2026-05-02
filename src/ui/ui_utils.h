#pragma once
#include "db/view.h"

MSCBL_API void refresh_results();

#define Orders(X) \
    X(Filename)   \
    X(Path)

#define X(a) OrderBy_##a,
enum OrderBy
{
    Orders(X) OrderBy_DISPLAY,
    OrderBy_Distance,
    OrderBy_COUNT
};
#undef X

#define X(a) #a,
local_v const char *order_str[] = {Orders(X)};
#undef X

// struct sort_params
// {
//     B32 ascending;
//     U64 limit;
//     OrderBy order_by;
//     Arena *arena;
// };
//
// MSCBL_API void index_fetch_order(sort_params params);

struct SizeUnits
{
    F32 size;
    const char *unit;
};

MSCBL_API SizeUnits formatted_size(U64 size);

#if DBG
void ui_reload();
#endif
