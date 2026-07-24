// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "base/log.h"
#include "db/fetch.h"
#include "base/array.h"
#include "db_helpers.h"
#include "gl/gl_core.h"
#include "base/arena.h"
#include "base/string.h"
#include "base/threadpool.h"

Atlas *atlases = NULL;
DirTree *dir_tree = NULL;
Arena *fetch_arena = NULL;

DBStmtCbk(push_atlas)
{
    S64 id = sqlite3_column_int64(stmt, 0);
    String path = string_copy(fetch_arena, sqlite3_column_text(stmt, 1));

    Atlas atl = {id};
    while (va_getsize(atlases) < id + 1)
        va_push(atlases, {0});

    if (!atlases[id].id)
    {
        atlases[id] = atl;
        AsyncTask task = {
            .func = gl_tex_path,
            .args = {
                {.kind = TPData_Any, .val_any = &atlases[id].tex},
                {.kind = TPData_String, .val_str = path},
            }};
        threadpool_enqueue(TaskPriority_Realtime, task);
    }
    else
        mscbl_log_info("Skipped atlas [%zu] %.*s", id, StringSpr(path));
}

// FIXME: loads directory names twice, need delete all and fill all or
// selectively fill
DBStmtCbk(push_dirs)
{
    S64 id = sqlite3_column_int64(stmt, 0);
    U64 level = (U64)sqlite3_column_int64(stmt, 1);
    String name = string_copy(fetch_arena, sqlite3_column_text(stmt, 2));

    DirTree node = {.id = id, .level = level, .name = name};
    DirKey new_idx = va_push(dir_tree, node);

    if (new_idx > 1)
    {
        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL)
        {
            S64 parent_dir = sqlite3_column_int64(stmt, 3);
            DirKey parent_idx = 0;
            for (S64 i = 1; i < va_getsize(dir_tree); i++)
            {
                if (dir_tree[i].id == parent_dir)
                {
                    parent_idx = i;
                    break;
                }
            }
            Assert(parent_idx, "shouldn't be possible");

            DirKey child_idx = dir_tree[parent_idx].child_idx;
            DirKey last_idx = child_idx;
            if (child_idx)
            {
                while (dir_tree[last_idx].next_idx)
                    last_idx = dir_tree[last_idx].next_idx;
                dir_tree[last_idx].next_idx = new_idx;
            }
            else
            {
                dir_tree[parent_idx].child_idx = new_idx;
            }
        }
        else
        {
            DirKey cur_root = 1;
            while (dir_tree[cur_root].next_idx)
                cur_root = dir_tree[cur_root].next_idx;
            dir_tree[cur_root].next_idx = new_idx;
        }
    }
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
    if (!va_getsize(dir_tree))
        va_push(dir_tree, {0});
    sqlite3_stmt *stmt = db_prepare("SELECT id, level, name, parent_dir FROM Dirs ORDER BY level;");
    db_run_stmt(stmt, 1, push_dirs);
}

OSString *db_fetch_pending()
{
    OSString *paths = NULL;
    sqlite3_stmt *stmt = db_prepare("SELECT path FROM DirSelect WHERE indexed = 0;");
#if OS_WIN32
    db_run_stmt(stmt, 1, push_pending16, &paths, fetch_arena);
#elif OS_LINUX
    db_run_stmt(stmt, 1, push_pending, &paths, fetch_arena);
#endif
    return paths;
}

ThreadFunc(db_fetchall)
{
    if (!fetch_arena)
        arena_alloc(MB(1), fetch_arena);

    db_fetch_dirtree();
    db_fetch_atlases();
}
