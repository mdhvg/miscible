// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "base/log.h"
#include "db/fetch.h"
#include "base/array.h"
#include "db_helpers.h"
#include "gl/gl_core.h"
#include "base/string.h"
#include "app/miscible.h"
#include "base/threadpool.h"

AtlasMap *fetch_atlases = NULL;
DirTree *fetch_directories = NULL;

DBStmtCbk(push_atlas)
{
    S64 id = sqlite3_column_int64(stmt, 0);
    String path = string_copy(app_arena, sqlite3_column_text(stmt, 1));

    AtlasMap atl = {id};
    while (arr_getsize(fetch_atlases) < id + 1)
        va_push(fetch_atlases, {0});

    if (!fetch_atlases[id].id)
    {
        fetch_atlases[id] = atl;
        AsyncTask task = {
            .func = gl_tex_path,
            .args = {
                {.kind = TPData_Any, .val_any = &fetch_atlases[id].tex},
                {.kind = TPData_String, .val_str = path},
            }};
        threadpool_enqueue(TaskPriority_Realtime, task);
    }
    else
        mscbl_log_info("Skipped atlas [%zu] %.*s", id, StringSpr(path));
}

DBStmtCbk(push_dirs)
{
    S64 id = sqlite3_column_int64(stmt, 0);
    U64 level = (U64)sqlite3_column_int64(stmt, 1);
    String name = string_copy(app_arena, sqlite3_column_text(stmt, 2));
    S64 parent_dir = sqlite3_column_int64(stmt, 3);

    if (!parent_dir)
    {
        DirKey cur_root = 1;
        while (fetch_directories[cur_root].next_idx)
            cur_root = fetch_directories[cur_root].next_idx;
        fetch_directories[cur_root].next_idx = id;
    }
    else
    {
        DirKey last_child = fetch_directories[parent_dir].child_idx;
        if (last_child)
        {
            while (fetch_directories[last_child].next_idx)
                last_child = fetch_directories[last_child].next_idx;
            fetch_directories[last_child].next_idx = id;
        }
        else
        {
            fetch_directories[parent_dir].child_idx = id;
        }
    }

    DirTree node = {.id = id, .level = level, .name = name};
    dense_update(fetch_directories, id, node);
}

DBStmtCbk(push_pending)
{
    String **paths = (String **)data;
    String path = string_copy(arena, sqlite3_column_text(stmt, 0));
    da_push(arena, *paths, path);
}

DBStmtCbk(push_pending16)
{
    WString **paths = (WString **)data;
    WString path = string_copy(arena, (wchar *)sqlite3_column_text16(stmt, 0));
    da_push(arena, *paths, path);
}

void db_fetch_atlases()
{
    sqlite3_stmt *stmt = db_prepare("SELECT id, atlas_path FROM Atlas;");
    db_run_stmt(stmt, 1, push_atlas);
}

void db_fetch_dirtree()
{
    if (!arr_getsize(fetch_directories))
        va_push(fetch_directories, {0});
    sqlite3_stmt *stmt = db_prepare("SELECT id, level, name, parent_dir FROM Dirs ORDER BY level;");
    db_run_stmt(stmt, 1, push_dirs);
}

OSString *db_fetch_pending()
{
    OSString *paths = NULL;
    sqlite3_stmt *stmt = db_prepare("SELECT path FROM DirSelect WHERE indexed = 0;");
#if OS_WIN32
    db_run_stmt(stmt, 1, push_pending16, &paths, app_arena);
#elif OS_LINUX
    db_run_stmt(stmt, 1, push_pending, &paths, app_arena);
#endif
    return paths;
}

ThreadFunc(db_fetchall)
{
    db_fetch_dirtree();
    db_fetch_atlases();
}
