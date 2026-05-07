#include "mscbl/init.h"

#include "db/fetch.h"
#include "db/view.h"
#include "scan/scan.h"
#include "base/array.h"
#include "db/db_helpers.h"

ThreadFunc(init_scan)
{
    arena_clear(arena);

    OSString *dirs = (OSString *)db_fetch_pending();
    sqlite3_stmt *stmt = db_prepare("UPDATE DirSelect SET indexed = 1 WHERE path = ?;");
    for (S64 i = 0; i < da_getsize(dirs); i++)
    {
        first_scan(arena, dirs[i]);

#if OS_WINDOWS
        sqlite3_bind_text16(stmt, 1, dirs[i].v, dirs[i].size * sizeof(wchar), SQLITE_STATIC);
#elif OS_LINUX
        sqlite3_bind_text(stmt, 1, dir.v, dir.size, SQLITE_STATIC);
#endif

        db_run_stmt(stmt);
    }
    sqlite3_finalize(stmt);

    view_reload();
}

ThreadFunc(init_atlas)
{
    ImageRow *inserted = NULL;

    sqlite3_stmt *stmt = db_prepare("SELECT id, path FROM Images WHERE atlas_id IS NULL;");
    db_run_stmt(stmt, 1, push_imagerow, &inserted, arena);
    scan_atlas_bake(arena, inserted);
}

void init_mscbl()
{
    threadpool_enqueue({init_scan});
    threadpool_enqueue({init_atlas});
    threadpool_enqueue({db_fetchall});
}
