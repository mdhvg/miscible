#include "miscible.h"

#include "base/arena.h"
#include "base/base_core.cpp"
#include "base/log.h"
#include "base/log.cpp"
#include "base/threadpool.h"
#include "gl/gl_core.h"
#include "os/os_inc.h"
#include "scan/scan.cpp"
#include "base/arena.cpp"
#include "base/string.cpp"
#include "config.cpp"
#include "base/threadpool.cpp"
#include "os/os_inc.cpp"
#include "db/db_helpers.cpp"
#include "window/window.cpp"
#include "ui/ui_core.cpp"
#include "db/fetch.cpp"
#include "db/view.cpp"
#include "base/ringbuf.cpp"
#include "mscbl/init.h"
#include "mscbl/init.cpp"
#include "yaml.cpp"
// #include "index/index.cpp"
// #include "index/index_dirtree.cpp"

#if defined(RDOC)
#include "renderdoc_app.h"
global_v RENDERDOC_API_1_6_0 *rdoc_api = NULL;
#if OS_WINDOWS
#elif OS_LINUX
#include <dlfcn.h>
#endif
#endif

Arena *mscbl_arena = NULL;

void prelaunch()
{
    os_prelaunch();
    config_init();
}

// TODO: Put this in Assert for Release build
#define CleanAndExit(c, ...) return ((__VA_ARGS__), c)

#define CLEANUP         \
    threadpool_free(),  \
        ui_close(),     \
        window_close(), \
        gl_close(),     \
        mscbl_log_deinit()

S32 mscbl_start(S32 argc, char **argv)
{
    // cli_args = make_default_args();

    // Parse arguments
    // if (!parse_args(argc, argv, &cli_args) || cli_args.help)
    // {
    //     print_help(argv[0]);
    //     return 1;
    // }

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

    prelaunch();

    mscbl_log_init();

    threadpool_init(os_info.worker_count);
    // TODO: Make a single task function (probably with a queue) in threadpool also
    if (!window_init())
        CleanAndExit(1, CLEANUP);

    db_make();
    ui_init();
    view_init();
    init_mscbl();

    while (win.active)
    {
        // Do non-UI things here
        window_poll();
        ui_update();
        ui_render();

        // Do stuff here for UI
        window_update();

        gl_pop();
    }

    CleanAndExit(0, CLEANUP);
}
