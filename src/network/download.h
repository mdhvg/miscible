#pragma once
#include "sha2.h"

#include "base/string.h"

struct DownloadArgs
{
    String link;
    String filepath;

    U64 file_size;
    U8 *file_hash;
};

Result download_file(Arena *arena, DownloadArgs args);
