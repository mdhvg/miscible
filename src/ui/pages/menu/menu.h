#pragma once
#include "db/view.h"
#include "ui/ui_core.h"

struct SortField
{
    SortType kind;
    const char *text;
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

global_v SortField sort_options[] = {
    {SortType_Path, "Path"},
    {SortType_Size, "Size"},
    {SortType_Filename, "Filename"},
    {SortType_DateCreated, "Date Created"},
    {SortType_DateModified, "Date Modified"},
    // {SortType_EmbeddingDistance, "EmbeddingDistance"},
};

global_v ViewFilter default_filter = {FilterType_Path};

// void new_filter(FilterList *list, FilterType type);
