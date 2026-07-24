// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include "os/os_inc.h"
#include "base/arena.h"
#include "base/base_core.h"

#include "os/os_inc.cpp"
#include "base/arena.cpp"
#include "base/base_core.cpp"

Arena *test_arena = NULL;

int main(int argc, char **argv)
{
    os_prelaunch();
    arena_alloc(MB(1), test_arena);

    doctest::Context context;
    context.applyCommandLine(argc, argv);
    int res = context.run();

    arena_free(test_arena);

    if (context.shouldExit())
        return res;
    return res;
}
