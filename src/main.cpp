#include "base/arena.h"
#include "base/task_runner.h"
#include "base/threadpool.h"
#include "os/os_inc.h"
#include "db/db_helpers.h"
#include "Images/dir_walker.h"
#include "window/window.h"
#include "ui/ui_core.h"
#include "inference/model.h"
#include "atlas/atlas_render.h"

// .c, .cpp
#include "base/base_core.cpp"
#include "base/arena.cpp"
#include "base/string.cpp"
#include "base/task_runner.cpp"
#include "base/threadpool.cpp"
#include "os/os_inc.cpp"
#include "db/db_helpers.cpp"
#include "Images/dir_walker.cpp"
#include "window/window.cpp"
#include "ui/ui_core.cpp"
#include "inference/model.cpp"
#include "base/array.cpp"
#include "atlas/atlas_render.cpp"

Application pics = {0};

#if defined(RDOC)
#include "renderdoc_app.h"
global RENDERDOC_API_1_6_0 *rdoc_api = NULL;
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

void print_dist(sqlite3_stmt *stmt, void *data)
{
	printf("Id: %zu, Path: %s, Dist: %.6f\n", sqlite3_column_int64(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_double(stmt, 2));
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
	pics.persistent_arena = arena_alloc(MB(256));
	pics.scratch		  = arena_alloc(MB(1));
	os_info.worker_count  = 2;
	os_info.pool		  = threadpool_init(pics.persistent_arena, os_info.worker_count);
	if (!window_init())
	{
		CleanAndExit(1, CLEANUP);
	}

	db_make();
	walk_directories(NULL);
	atlas_render_prepare();
	ui_init();
	model_init();

	// F32 *sample = push_array(pics.persistent_arena, 512, F32);
	// db_run("SELECT Images.embedding FROM Images LIMIT 1;", get_sample, sample);
	// sqlite3_stmt *stmt = db_prepare("SELECT id, path, distance_cosine_f32(embedding, ?) AS distance FROM Images WHERE embedding IS NOT NULL AND distance < 0.5 ORDER BY distance ASC;");
	// sqlite3_bind_blob(stmt, 1, sample, 512 * sizeof(F32), SQLITE_STATIC);
	// db_run_stmt(stmt, 1, print_dist);

	push_task(TASK_LOAD_THUMBNAILS);
	push_task(TASK_DRAW_THUMBNAILS);
	push_task(TASK_LOAD_ATLAS);
	push_task(TASK_EMBED_IMAGES);

	while (win.active)
	{
		task_runner_try(os_info.pool);
		// Do non-UI things here
		window_poll();
		ui_update();
		ui_render();
		// Do stuff here for UI
		window_update();
	}

	CleanAndExit(0, CLEANUP);
}
