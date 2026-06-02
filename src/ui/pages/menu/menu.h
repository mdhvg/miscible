#pragma once
#include "IconsLucide.h"

#include "db/view.h"

struct SortField
{
    SortType kind;
    const char *text;
};

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

struct ZoomLevel
{
    Zoom level;
    S32 size;
    const char *text;
};

global_v SortField sort_options[] = {
    {SortType_Path, "Path"},
    {SortType_Size, "Size"},
    {SortType_Filename, "Filename"},
    {SortType_DateCreated, "Date Created"},
    {SortType_DateModified, "Date Modified"},
};

global_v ZoomLevel zoom_options[] = {
    {Zoom_Small, 64, ICON_LC_GRID_3X3 " Small"},
    {Zoom_Medium, 128, ICON_LC_GRID_2X2 " Medium"},
    {Zoom_Large, 256, ICON_LC_SQUARE " Large"},
};

F32 filter_input_field_units = 32.0f;
F32 sidebar_fold_units = 12.0f;

F32 min_search_input_units = 30.0f;
F32 max_search_input_units = 850.0f;

S64 max_semantic_results = 20;
F32 filter_accent_width = 4.0f;
F32 sidebar_open_units = 80.0f;
