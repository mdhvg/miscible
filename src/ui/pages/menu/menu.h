// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include "IconsLucide.h"

#include "db/view.h"
#include "imgui.h"

enum Zoom
{
    Zoom_Small,
    Zoom_Medium,
    Zoom_Large,
    Zoom_COUNT,
};

enum FilterInput
{
    FilterInput_None,
    FilterInput_String,
    FilterInput_Int,
    FilterInput_Bytes,
    FilterInput_Date,
    FilterInput_Float,
};

struct SortOption
{
    SortType type;
    const char *text;
};

struct ZoomOption
{
    Zoom level;
    S32 size;
    const char *text;
};

struct SearchSelection
{
    SearchType type;
    const char *info_text;
    const char *button_text;
};

struct GroupOption
{
    GroupSubType type;
    const char *text;
};

global_v SortOption sort_options[] = {
    {.type = SortType_Directory, .text = "Folder"},
    {.type = SortType_Filename, .text = "Filename"},
    {.type = SortType_Size, .text = "Size"},
    {.type = SortType_DateAdded, .text = "Date Added"},
    {.type = SortType_DateCreated, .text = "Date Created"},
    {.type = SortType_DateModified, .text = "Date Modified"},
};

global_v ZoomOption zoom_options[] = {
    {.level = Zoom_Small, .size = 64, .text = ICON_LC_GRID_3X3 " Small"},
    {.level = Zoom_Medium, .size = 128, .text = ICON_LC_GRID_2X2 " Medium"},
    {.level = Zoom_Large, .size = 256, .text = ICON_LC_SQUARE " Large"},
};

global_v SearchSelection search_options[] = {
    {.type = SearchType_Text, .info_text = "Search using text in filename", .button_text = "Filename"},
    {.type = SearchType_Embedding, .info_text = "Search by image description", .button_text = "Context"},
};

global_v GroupOption size_group_options[] = {
    {.type = Group_Size1KB, .text = "1 KB"},
    {.type = Group_Size10KB, .text = "10 KB"},
    {.type = Group_Size100KB, .text = "100 KB"},
    {.type = Group_Size1MB, .text = "1 MB"},
    {.type = Group_Size10MB, .text = "10 MB"},
    {.type = Group_Size100MB, .text = "100 MB"},
    {.type = Group_Size1GB, .text = "1 GB"},
};

global_v GroupOption date_group_options[] = {
    {.type = Group_DateDay, .text = "Day"},
    {.type = Group_DateMonth, .text = "Month"},
    {.type = Group_DateYear, .text = "Year"},
};

local_v Zoom zoom_level = Zoom_Medium;

local_v F32 topbar_height_units = 10.0f;

local_v F32 sidebar_fold_units = 12.0f;
local_v F32 sidebar_open_units = 80.0f;

local_v F32 filter_accent_width = 4.0f;

local_v F32 min_search_input_units = 30.0f;
local_v F32 max_search_input_units = 850.0f;
