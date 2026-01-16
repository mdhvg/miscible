#include "miscible.h"
#include "miscible.cpp"

#if defined(RDOC)
#include "renderdoc_app.h"
global_v RENDERDOC_API_1_6_0 *rdoc_api = NULL;
#if OS_WINDOWS
#elif OS_LINUX
#include <dlfcn.h>
#endif
#endif

// TODO: Put this in Assert for Release build
#define CleanAndExit(c, ...) return ((__VA_ARGS__), c)

#define CLEANUP                    \
    threadpool_free(os_info.pool), \
        ui_close(),                \
        window_close()

DB_CALLBACK(get_sample)
{
    MemoryCopy(data, argv[0], 512 * sizeof(F32));
    return 0;
}

int main(int, char **)
{
#if defined(RDOC)
#if OS_WINDOWS
#elif OS_LINUX
    if (void *mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
        ASSERT(RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void **)&rdoc_api) == 1);
    }
#endif
#endif

    os_prelaunch();

    mscbl.persistent_arena = arena_alloc(MB(100));
    mscbl.scratch          = arena_alloc(MB(1));
    os_info.worker_count   = 2;
    os_info.pool           = threadpool_init(mscbl.persistent_arena, os_info.worker_count);
    // TODO: Make a single task function (probably with a queue) in threadpool also
    if (!window_init())
    {
        CleanAndExit(1, CLEANUP);
    }

    db_make();
    // walk_directories();
    // atlas_render_prepare();
    ui_init();
    scan_restart();
    // model_init();

    // F32 *sample = push_array(mscbl.persistent_arena, 512, F32);
    // db_run("SELECT Images.embedding FROM Images LIMIT 1;", get_sample, sample);
    // sqlite3_stmt *stmt = db_prepare("SELECT id, path, distance_cosine_f32(embedding, ?) AS distance FROM Images WHERE embedding IS NOT NULL AND distance < 0.5 ORDER BY distance ASC;");
    // sqlite3_bind_blob(stmt, 1, sample, 512 * sizeof(F32), SQLITE_STATIC);
    // db_run_stmt(stmt, 1, print_dist);

    // push_task(TASK_LOAD_THUMBNAILS);
    // push_task(TASK_DRAW_THUMBNAILS);
    // push_task(TASK_LOAD_ATLAS);
    // push_task(TASK_EMBED_IMAGES);

    while (win.active)
    {
        // task_runner_try(os_info.pool);
        // if (refresh)
        //      scan_restart();
        scan_update();
        // // Do non-UI things here
        window_poll();
        ui_update();
        ui_render();
        // Do stuff here for UI
        window_update();
    }

    CleanAndExit(0, CLEANUP);
}
