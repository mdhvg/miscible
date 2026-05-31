#pragma once
#include "IconsMaterialSymbols.h"

#include "db/view.h"
#include "db/fetch.h"

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
    {Zoom_Small, 128, ICON_MS_BACKGROUND_GRID_SMALL " Small"},
    {Zoom_Medium, 256, ICON_MS_GRID_ON " Medium"},
    {Zoom_Large, 512, ICON_MS_WINDOW " Large"},
};

F32 default_grid_spacing_units = 0.5f;
F32 filter_input_field_units = 32.0f;
F32 sidebar_collapsed_units = 12.0f;
F32 min_search_input_units = 30.0f;
S64 max_semantic_results = 20;
F32 filter_accent_width = 4.0f;
F32 sidebar_open_units = 80.0f;
