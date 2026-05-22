#include "base/array.h"
#include "db/fetch.h"
#include "db/view.h"
#include "scan/scan.h"
#include "base/string.h"
#include "base/threadpool.h"
#include "inference/model.h"
#include "sqlite3.h"

#if OS_WINDOWS
#include "os/win32/win32_scan_dirs.cpp"
#elif OS_LINUX
#include "os/linux/linux_scan_dirs.cpp"
#endif

DBStmtCbk(push_imagerow)
{
    ImageRow **inserted = (ImageRow **)data;
    ImageRow row = {
        .id = sqlite3_column_int64(stmt, 0),
        .path = string_copy(arena, sqlite3_column_text(stmt, 1))};
    da_push(arena, *inserted, row);
}

ThreadFunc(scan_routine)
{
    Assert(data.kind == TPData_OSString, "wrong datatype");
    OSString dir = data.val_os_str;

    first_scan(arena, dir);
    view_reload();

    sqlite3_stmt *stmt = db_prepare("UPDATE DirSelect SET indexed = 1 WHERE path = ?;");
#if OS_WINDOWS
    sqlite3_bind_text16(stmt, 1, dir.v, dir.size * sizeof(wchar), SQLITE_STATIC);
#elif OS_LINUX
    sqlite3_bind_text(stmt, 1, dir.v, dir.size, SQLITE_STATIC);
#endif
    db_run_stmt(stmt, 1);

    ImageRow *inserted = NULL;

    stmt = db_prepare("SELECT id, path FROM Images WHERE atlas_id IS NULL;");
    db_run_stmt(stmt, 1, push_imagerow, &inserted, arena);
    scan_atlas_bake(arena, inserted);

    threadpool_enqueue({.func = db_fetchall});
    threadpool_enqueue({.func = model_insert_embedding});
}

void scan_new_dir()
{
    OSString dir = os_select_dir(W("Select Directory"), NULL);
    if (dir.size)
    {
        sqlite3_stmt *stmt = db_prepare("INSERT INTO DirSelect(path) VALUES(?);");
#if OS_WINDOWS
        sqlite3_bind_text16(stmt, 1, dir.v, dir.size * sizeof(wchar), SQLITE_STATIC);
#elif OS_LINUX
        sqlite3_bind_text(stmt, 1, dir.v, dir.size, SQLITE_STATIC);
#endif
        db_run_stmt(stmt, 1);

        TPData args = {.kind = TPData_OSString, .val_os_str = dir};
        threadpool_enqueue({scan_routine, args});
    }
}
