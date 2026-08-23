// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include "sha2.h"

#include "base/string.h"

struct DownloadArgs
{
    String link;
    String file_path;

    U64 file_size;
    U8 *file_hash;
};

Result download_file(Arena *arena, DownloadArgs args);
