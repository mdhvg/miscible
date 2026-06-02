#pragma once
#include "base/base_core.h"

struct SizeUnits
{
    F32 size;
    const char *unit;
};

MSCBL_API SizeUnits formatted_size(U64 size);

#if DBG
void ui_reload();
#endif
