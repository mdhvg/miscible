#include "os/os_inc.h"
#include "scan/scan.h"
#include "base/array.h"
#include "base/arena.h"
#include "base/string.h"
#include "config.h"
#include "base/log.h"
#include "base/threadpool.h"
#include "db/db_helpers.h"
#include "base/arena.h"
#include "sqlite3.h"

struct DirInfo
{
    S64 id;
    U64 mtime;
    U64 level;
    S64 root_id;
    S64 parent_id;
    WString name;
    WString path;
};

local_v const char *dir_insert_query =
    "INSERT INTO Dirs(path, name, mtime, level, root_id, parent_id) "
    "VALUES(?, ?, ?, ?, ?, ?) "
    "ON CONFLICT(path) DO UPDATE SET "
    "  name         = excluded.name, "
    "  mtime        = excluded.mtime, "
    "  level        = excluded.level, "
    "  root_id      = excluded.root_id, "
    "  parent_id    = excluded.parent_id "
    "WHERE excluded.mtime > Dirs.mtime "
    "RETURNING id;";

local_v const char *img_insert_query =
    "INSERT INTO Images(path, filename, size, mtime, ctime, root_id, parent_id) "
    "VALUES(? || \"\\\" || ?, ?, ?, ?, ?, ?, ?) "
    "ON CONFLICT(path) DO UPDATE SET "
    "  filename     = excluded.filename, "
    "  size         = excluded.size, "
    "  mtime        = excluded.mtime, "
    "  ctime        = excluded.ctime, "
    "  root_id      = excluded.root_id, "
    "  parent_id    = excluded.parent_id "
    "WHERE excluded.mtime > Images.mtime "
    "RETURNING id, path;";

local_v StringBuilder scratch_str = {0};
local_v sqlite3_stmt *dir_stmt    = NULL;
local_v sqlite3_stmt *img_stmt    = NULL;

/*
 *******************************************************************************
                                 First scan
 *******************************************************************************
*/

DBStmtCbk(get_row)
{
    ImageRow *row = (ImageRow *)data;
    row->id       = sqlite3_column_int64(stmt, 0);
    row->path     = string_copy(arena, sqlite3_column_text(stmt, 1));
}

// No allocations per stack call
void recursive_insert(Arena *arena, DirInfo dir)
{
    if (dir.level > mscbl_config.settings.scan_depth)
        return;

    // Insert current dir
    sqlite3_bind_text16(dir_stmt, 1, WCStrCast(dir.path), dir.path.size * sizeof(wchar), SQLITE_STATIC);
    sqlite3_bind_text16(dir_stmt, 2, WCStrCast(dir.name), dir.name.size * sizeof(wchar), SQLITE_STATIC);
    sqlite3_bind_int64(dir_stmt, 3, dir.mtime);
    sqlite3_bind_int64(dir_stmt, 4, dir.level);
    if (dir.root_id >= 0)
        sqlite3_bind_int64(dir_stmt, 5, dir.root_id);
    if (dir.parent_id >= 0)
        sqlite3_bind_int64(dir_stmt, 6, dir.parent_id);
    if (!db_run_stmt(dir_stmt, 0, get_id, &dir.id, arena))
        return; // Dir already exists (no need to scan again)

    sqlite3_reset(dir_stmt);
    sqlite3_clear_bindings(dir_stmt);

    if (dir.root_id < 0)
        dir.root_id = dir.id;

    // Loop over files in current dir
    WIN32_FIND_DATAW fdFile;
    HANDLE hFind = NULL;

    WString expression = string_formatw(&scratch_str, L"%.*ls\\*.*", WStringSpr(dir.path));
    hFind              = FindFirstFileW(WCStrCast(expression), &fdFile);

    do
    {
        if (hFind == INVALID_HANDLE_VALUE)
            break;

        WString filename = sv(fdFile.cFileName);

        if (match_front(filename, L".") ||
            match_front(filename, L".."))
            continue;

        mscbl_log_dbg("path: %.*ls", WStringSpr(filename));

        if (fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            // Recurse into a dir if found
            FILETIME write_time = fdFile.ftLastWriteTime;
            U64 mtime           = 0;
            mtime               = write_time.dwHighDateTime;
            mtime <<= 32;
            mtime |= write_time.dwLowDateTime;

            WString path = string_formatw(&scratch_str, L"%.*ls\\%.*ls", WStringSpr(dir.path), WStringSpr(filename));
            DirInfo next = {.id        = 0,
                            .mtime     = mtime,
                            .level     = dir.level + 1,
                            .root_id   = dir.root_id,
                            .parent_id = dir.id,
                            .name      = filename,
                            .path      = path};
            recursive_insert(arena, next);
        }
        else
        {
            if (!match_end(filename, L".jpg") && !match_end(filename, L".png"))
                continue;

            FILETIME make_time  = fdFile.ftCreationTime;
            FILETIME write_time = fdFile.ftLastWriteTime;
            U64 mtime = 0, ctime = 0, size = 0;
            mtime = write_time.dwHighDateTime;
            mtime <<= 32;
            mtime |= write_time.dwLowDateTime;
            ctime = make_time.dwHighDateTime;
            ctime <<= 32;
            ctime |= make_time.dwLowDateTime;
            size = fdFile.nFileSizeHigh;
            size <<= 32;
            size |= fdFile.nFileSizeLow;

            sqlite3_bind_text16(img_stmt, 1, dir.path.v, dir.path.size * sizeof(wchar), SQLITE_STATIC);
            sqlite3_bind_text16(img_stmt, 2, WCStrCast(filename), filename.size * sizeof(wchar), SQLITE_STATIC);
            sqlite3_bind_text16(img_stmt, 3, WCStrCast(filename), filename.size * sizeof(wchar), SQLITE_STATIC);
            sqlite3_bind_int64(img_stmt, 4, size);
            sqlite3_bind_int64(img_stmt, 5, mtime);
            sqlite3_bind_int64(img_stmt, 6, ctime);
            sqlite3_bind_int64(img_stmt, 7, dir.root_id);
            sqlite3_bind_int64(img_stmt, 8, dir.id);

            ImageRow row = {0};
            if (db_run_stmt(img_stmt, 0, get_row, &row, arena) > 0)
            {
                // Then something was inserted
            }
            sqlite3_reset(img_stmt);
            sqlite3_clear_bindings(img_stmt);
        }
    } while (FindNextFileW(hFind, &fdFile));
    FindClose(hFind);
}

void first_scan(Arena *arena, WString dir)
{
    arena_clear(arena);
    if (!scratch_str.capacity)
        scratch_str = string_empty(arena, 1024);

    WString path = string_copy(arena, dir);

    dir_stmt = db_prepare(dir_insert_query);
    img_stmt = db_prepare(img_insert_query);

    db_run("BEGIN TRANSACTION;");

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    B32 res = GetFileAttributesExW(WCStrCast(path), GetFileExInfoStandard, &attrs);

    // TODO: Actually delete it from db if it exists
    Assert(res, "OS stat error (path:%.*ls)", WStringSpr(path));

    // Insert root dir
    FILETIME ftime = attrs.ftLastWriteTime;
    U64 mtime      = 0;
    mtime          = ftime.dwHighDateTime;
    mtime <<= 32;
    mtime |= ftime.dwLowDateTime;

    WString filename = path;
    S64 name_start   = string_rfind(filename, L'\\');
    Assert(name_start >= 0, "couldn't find \\ in path");
    filename.v += name_start + 1;
    filename.size -= name_start + 1;

    recursive_insert(arena,
                     {.id        = 0,
                      .mtime     = mtime,
                      .level     = 0,
                      .root_id   = -1,
                      .parent_id = -1,
                      .name      = filename,
                      .path      = path});

    sqlite3_finalize(dir_stmt);
    sqlite3_finalize(img_stmt);

    db_run("COMMIT;");
}

/*
 *******************************************************************************
                        Continuous scan (subsequent times)
 *******************************************************************************
*/

DirInfo *saved_dirs = NULL;

B32 check_dir(DirInfo dir)
{
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    B32 res = GetFileAttributesExW(WCStrCast(dir.path), GetFileExInfoStandard, &attrs);

    if (!res)
    {
        // TODO: Actually delete it from db if it exists
        mscbl_log_error("OS stat error (path:%.*ls)", WStringSpr(dir.path));
        return 1;
    }

    FILETIME ftime = attrs.ftLastWriteTime;
    U64 mtime      = 0;
    mtime          = ftime.dwHighDateTime;
    mtime <<= 32;
    mtime |= ftime.dwLowDateTime;

    return dir.mtime >= mtime;
}

DBStmtCbk(push_paths)
{
    DirInfo info = {
        .id        = sqlite3_column_int64(stmt, 0),
        .mtime     = (U64)sqlite3_column_int64(stmt, 1),
        .level     = (U64)sqlite3_column_int64(stmt, 2),
        .root_id   = sqlite3_column_int64(stmt, 3),
        .parent_id = sqlite3_column_int64(stmt, 4),
        .name      = string_copy(arena, (wchar *)sqlite3_column_text16(stmt, 5)),
        .path      = string_copy(arena, (wchar *)sqlite3_column_text16(stmt, 6))};
    da_push(arena, saved_dirs, info);
}

void cont_scan(Arena *arena)
{
    arena_clear(arena);
    if (!scratch_str.capacity)
        scratch_str = string_empty(arena, 1024);

    /*************************** Global scan init *****************************/
    // if (!scan_arena)
    //     arena_alloc(MB(1), scan_arena);
    // if (!scan_state.inserted)
    //     da_setcap(scan_arena, scan_state.inserted, Kil(10));
    // da_clear(scan_state.inserted);
    /**************************************************************************/

    dir_stmt = db_prepare(dir_insert_query);
    img_stmt = db_prepare(img_insert_query);

    // Get all previous entries
    saved_dirs = NULL;
    db_run_stmt(db_prepare("SELECT id, mtime, level, root_id, parent_id, name, path FROM Dirs;"), 1, push_paths, NULL, arena);
    // Loop over them and stat
    for (S64 i = 0; i < da_getsize(saved_dirs); i++)
        if (!check_dir(saved_dirs[i]))
            recursive_insert(arena, saved_dirs[i]);

    sqlite3_finalize(dir_stmt);
    sqlite3_finalize(img_stmt);
}
