// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include <atomic>
#include <memory>
#include "app/miscible.h"

#include "base/log.h"
#include "base/threadpool.h"
#include "config.h"
#include "os/os_inc.h"
#include "base/string.cpp"
#include "config.cpp"
#include "os/os_inc.cpp"
#include "db/db_helpers.cpp"
#include "scan/scan.h"
#include "scan/scan_dirs.cpp"
#include "scan/scan_atlas.cpp"
#include "inference/clip.cpp"
#include "inference/model.cpp"
#include "window/window.cpp"
#include "ui/ui_core.cpp"
#include "db/fetch.cpp"
#include "db/view.cpp"
#include "yaml.cpp"
#include "network/download.cpp"
#include "base/log.cpp"
#include "base/arena.cpp"
#include "base/threadpool.cpp"
#include "base/base_core.cpp"
#include "inference/inference.cpp"

#if defined(RDOC)
#include "renderdoc_app.h"
global_v RENDERDOC_API_1_6_0 *rdoc_api = NULL;
#if OS_WIN32
#elif OS_LINUX
#include <dlfcn.h>
#endif
#endif

Arena *mscbl_arena = NULL;

ThreadFunc(init_scan)
{
    arena_clear(arena);

    OSString *dirs = (OSString *)db_fetch_pending();
    sqlite3_stmt *stmt = db_prepare("UPDATE DirSelect SET indexed = 1 WHERE path = ?;");
    for (S64 i = 0; i < da_getsize(dirs); i++)
    {
        first_scan(arena, dirs[i]);

#if OS_WIN32
        sqlite3_bind_text16(stmt, 1, dirs[i].v, dirs[i].size * sizeof(wchar), SQLITE_STATIC);
#elif OS_LINUX
        sqlite3_bind_text(stmt, 1, dir.v, dir.size, SQLITE_STATIC);
#endif

        db_run_stmt(stmt);
    }
    sqlite3_finalize(stmt);

    // view_refresh();

    // TODO: run a continuous scan afterwards
}

ThreadFunc(init_atlas)
{
    ImageRow *inserted = NULL;

    sqlite3_stmt *stmt = db_prepare("SELECT id, path FROM Images WHERE atlas_id IS NULL;");
    db_run_stmt(stmt, 1, push_imagerow, &inserted, arena);
    scan_atlas_bake(arena, inserted);
}

S32 mscbl_start(S32 argc, char **argv)
{
#if defined(RDOC)
#if OS_WIN32
#elif OS_LINUX
    if (void *mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
        ASSERT(RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void **)&rdoc_api) == 1);
    }
#endif
#endif

    os_prelaunch();
    config_init();

    mscbl_log_init(mscbl_config.settings.log_age_days);

    threadpool_init(os_info.worker_count);
    window_init();

    db_make();
    ui_init();
    view_init();
    inference_init();

    threadpool_enqueue(TaskPriority_High, {.func = init_scan});
    threadpool_enqueue(TaskPriority_High, {.func = init_atlas});
    threadpool_enqueue(TaskPriority_High, {.func = db_fetchall});
    threadpool_enqueue(TaskPriority_Low, {.func = inference_backend_init});

    while (win.active)
    {
        if (window_poll())
        {
            ui_update();
            ui_render();
            window_update();
        }
    }

    ui_close();
    window_shutdown();
    inference_close();
    threadpool_free();
    mscbl_log_deinit();

    return 0;
}
