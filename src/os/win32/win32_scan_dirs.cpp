// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "base/arena.h"
#include "base/wstring.h"
#include "config.h"
#include "base/log.h"
#include "scan/scan.h"
#include "base/array.h"
#include "base/string.h"
#include "db/db_helpers.h"

// TODO: Make it ignore windows hidden dirs like $RECYCLEBIN, etc and also find
// a way to avoid recursive symlinks

// TODO: replace findfirstfile with GetFileInformationByHandleEx to avoid patten
// construction

struct DirInfo
{
    S64 id;
    U64 mtime;
    U64 level;
    S64 root_dir;
    S64 parent_dir;
    Str16 name;
    Str16 path;
};

local_v const char *dir_insert_query =
    "INSERT INTO Dirs(path, name, mtime, level, root_dir, parent_dir) "
    "VALUES(?, ?, ?, ?, ?, ?) "
    "ON CONFLICT(path) DO UPDATE SET "
    "  name         = excluded.name, "
    "  mtime        = excluded.mtime, "
    "  level        = excluded.level, "
    "  root_dir     = excluded.root_dir, "
    "  parent_dir   = excluded.parent_dir "
    "WHERE excluded.mtime > Dirs.mtime "
    "RETURNING id;";

local_v const char *img_insert_query =
    "INSERT INTO Images(path, filename, size, mtime, ctime, root_dir, parent_dir) "
    "VALUES(? || '" OSSlash "' || ?, ?, ?, ?, ?, ?, ?) "
    "ON CONFLICT(path) DO UPDATE SET "
    "  filename     = excluded.filename, "
    "  size         = excluded.size, "
    "  mtime        = excluded.mtime, "
    "  ctime        = excluded.ctime, "
    "  root_dir     = excluded.root_dir, "
    "  parent_dir   = excluded.parent_dir "
    "WHERE excluded.mtime > Images.mtime "
    "RETURNING id, path;";

local_v sqlite3_stmt *dir_stmt = NULL;
local_v sqlite3_stmt *img_stmt = NULL;

/*
 *******************************************************************************
                                 First scan
 *******************************************************************************
*/

DBStmtCbk(get_row)
{
    ImageRow *row = (ImageRow *)data;
    row->id = sqlite3_column_int64(stmt, 0);
    row->path = str8_copy(arena, sqlite3_column_text(stmt, 1));
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

    // Only bind parent address if provided (>=0 value)
    if (dir.root_dir >= 0) sqlite3_bind_int64(dir_stmt, 5, dir.root_dir);
    if (dir.parent_dir >= 0) sqlite3_bind_int64(dir_stmt, 6, dir.parent_dir);

    if (!db_run_stmt(dir_stmt, 0, get_id, &dir.id, arena)) return; // Dir already exists (no need to scan again)

    sqlite3_reset(dir_stmt);
    sqlite3_clear_bindings(dir_stmt);

    if (dir.root_dir < 0) dir.root_dir = dir.id;

    // Loop over files in current dir
    HANDLE dirfd = CreateFileW(WCStrCast(dir.path), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (dirfd == INVALID_HANDLE_VALUE)
    {
        mscbl_log_error("Error code: %u (path: %.*ls)", GetLastError(), Str16Spr(dir.path));
        return;
    }

    U64 buffer_size = KB(64);
    U8 *buffer = push_array(arena, buffer_size, U8);

    FILE_INFO_BY_HANDLE_CLASS infoClass = FileIdBothDirectoryRestartInfo;
    while (GetFileInformationByHandleEx(dirfd, infoClass, buffer, buffer_size))
    {
        for (FILE_ID_BOTH_DIR_INFO *info = (FILE_ID_BOTH_DIR_INFO *)buffer; info->NextEntryOffset != 0; info += info->NextEntryOffset)
        {
            Str16 filename = sv16(info->FileName, info->FileNameLength);
            if (str16_match_beg(filename, sv16_lit(L".")) || str16_match_beg(filename, sv16_lit(L".."))) continue;

            mscbl_log_info("path: %.*ls", Str16Spr(filename));

            if (info->FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                // Recurse into a dir if found
                U64 mtime = info->LastWriteTime.QuadPart;
                win32_to_unix_timestamp(&mtime);

                StringBuilder new_path = str16_init(arena, dir.path);
                str16_path_join(&new_path, filename);

                DirInfo next = {.id = 0,
                                .mtime = mtime,
                                .level = dir.level + 1,
                                .root_dir = dir.root_dir,
                                .parent_dir = dir.id,
                                .name = filename,
                                .path = Str16Cast(new_path)};
                ArenaScoped(arena) recursive_insert(arena, next);
            }
            else
            {
                if (!str16_match_end(filename, sv16_lit(L".png"), StringMatch_IgnoreCase) &&
                    !str16_match_end(filename, sv16_lit(L".jpg"), StringMatch_IgnoreCase) &&
                    !str16_match_end(filename, sv16_lit(L".jpeg"), StringMatch_IgnoreCase))
                    continue;

                U64 size = info->EndOfFile.QuadPart;
                U64 mtime = info->LastWriteTime.QuadPart;
                U64 ctime = info->CreationTime.QuadPart;

                win32_to_unix_timestamp(&mtime);
                win32_to_unix_timestamp(&ctime);

                sqlite3_bind_text16(img_stmt, 1, WCStrCast(dir.path), dir.path.size * sizeof(wchar), SQLITE_STATIC);
                sqlite3_bind_text16(img_stmt, 2, WCStrCast(filename), filename.size * sizeof(wchar), SQLITE_STATIC);
                sqlite3_bind_text16(img_stmt, 3, WCStrCast(filename), filename.size * sizeof(wchar), SQLITE_STATIC);
                sqlite3_bind_int64(img_stmt, 4, size);
                sqlite3_bind_int64(img_stmt, 5, mtime);
                sqlite3_bind_int64(img_stmt, 6, ctime);
                sqlite3_bind_int64(img_stmt, 7, dir.root_dir);
                sqlite3_bind_int64(img_stmt, 8, dir.id);

                ImageRow row = {0};
                if (db_run_stmt(img_stmt, 0, get_row, &row, arena) > 0)
                {
                    // Then something was inserted
                }
                sqlite3_reset(img_stmt);
                sqlite3_clear_bindings(img_stmt);
            }
        }

        infoClass = FileIdBothDirectoryInfo;
    }
}

void first_scan(Arena *arena, Str16 dir)
{
    arena_clear(arena);
    Str16 path = str16_copy(arena, dir);

    dir_stmt = db_prepare(dir_insert_query);
    img_stmt = db_prepare(img_insert_query);

    db_run("BEGIN TRANSACTION;");

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    // TODO: Actually delete it from db if it exists
    Assert(GetFileAttributesExW(WCStrCast(path), GetFileExInfoStandard, &attrs), "OS stat error (path:%.*ls)", Str16Spr(path));

    // Insert root dir
    FILETIME ftime = attrs.ftLastWriteTime;
    U64 mtime = 0;
    mtime = ftime.dwHighDateTime;
    mtime <<= 32;
    mtime |= ftime.dwLowDateTime;
    win32_to_unix_timestamp(&mtime);

    S64 name_start = -1;
    Assert(str16_rfind(path, sv16_lit(L"\\"), 0, &name_start), "couldn't find \\ in %.*ls", Str16Spr(path));
    Str16 filename = str16_from(path, name_start + 1);

    DirInfo next = {.id = 0,
                    .mtime = mtime,
                    .level = 0,
                    .root_dir = -1,
                    .parent_dir = -1,
                    .name = filename,
                    .path = path};
    ArenaScoped(arena) recursive_insert(arena, next);

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
        mscbl_log_error("OS stat error (path:%.*ls)", Str16Spr(dir.path));
        return 1;
    }

    FILETIME ftime = attrs.ftLastWriteTime;
    U64 mtime = 0;
    mtime = ftime.dwHighDateTime;
    mtime <<= 32;
    mtime |= ftime.dwLowDateTime;
    win32_to_unix_timestamp(&mtime);

    return dir.mtime >= mtime;
}

DBStmtCbk(push_paths)
{
    DirInfo info = {
        .id = sqlite3_column_int64(stmt, 0),
        .mtime = (U64)sqlite3_column_int64(stmt, 1),
        .level = (U64)sqlite3_column_int64(stmt, 2),
        .root_dir = sqlite3_column_int64(stmt, 3),
        .parent_dir = sqlite3_column_int64(stmt, 4),
        .name = str16_copy(arena, (wchar *)sqlite3_column_text16(stmt, 5)),
        .path = str16_copy(arena, (wchar *)sqlite3_column_text16(stmt, 6))};
    da_push(arena, saved_dirs, info);
}

void cont_scan(Arena *arena)
{
    arena_clear(arena);

    dir_stmt = db_prepare(dir_insert_query);
    img_stmt = db_prepare(img_insert_query);

    // Get all previous entries
    saved_dirs = NULL;
    db_run_stmt(db_prepare("SELECT id, mtime, level, root_dir, parent_dir, name, path FROM Dirs;"), 1, push_paths, NULL, arena);
    // Loop over them and stat
    for (S64 i = 0; i < arr_getsize(saved_dirs); i++)
    {
        if (!check_dir(saved_dirs[i]))
        {
            ArenaScoped(arena)
            {
                recursive_insert(arena, saved_dirs[i]);
            }
        }
    }

    sqlite3_finalize(dir_stmt);
    sqlite3_finalize(img_stmt);
}
