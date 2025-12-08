#include "base/core.h"
#include "base/arena.h"
#include "base/string.h"
#include "base/threadpool.h"
#include "os/os_inc.h"
#include "db/db_helpers.h"
#include "atlas/atlas_render.h"
#include "Images/dir_walker.h"

#include "base/core.cpp"
#include "base/arena.cpp"
#include "base/string.cpp"
#include "base/threadpool.cpp"
#include "os/os_inc.cpp"
#include "db/db_helpers.cpp"
#include "Images/dir_walker.cpp"
#include "atlas/atlas_render.cpp"

#if defined(RDOC)
#include "renderdoc_app.h"
global RENDERDOC_API_1_6_0 *rdoc_api = NULL;
#if OS_WINDOWS
#elif OS_LINUX
#include <dlfcn.h>
#endif
#endif

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
	persistent_arena = arena_alloc(MB(256));
	ThreadPool *pool = threadpool_init(persistent_arena, 16);

	db_make(NULL);
	walk_directories(NULL);
	atlas_init(NULL);

	U64 z = 0;
	parallel_for(pool, 100, load_thumbnails, (void *)&z);

	// Wait here
	S32 i = 0;
	scanf("%d", &i);

	threadpool_free(pool);
	return 0;
}
