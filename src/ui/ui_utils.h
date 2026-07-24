// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

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
