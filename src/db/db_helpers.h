// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include "sqlite3.h"

#include "base/string.h"

#define DBStmtCbk(name) void name(sqlite3_stmt *stmt, void *data, Arena *arena)

DBStmtCbk(get_count);
DBStmtCbk(get_id);
DBStmtCbk(get_path);

typedef DBStmtCbk(DBStCbk);

void db_init();
void db_run(const char *command, sqlite3_callback callback = NULL, void *data = NULL);
inline void db_run(String command, sqlite3_callback callback = NULL, void *data = NULL)
{
    db_run(CStrCast(command), callback, data);
}
MSCBL_API sqlite3_stmt *db_prepare(const char *sql);
MSCBL_API U64 db_run_stmt(sqlite3_stmt *stmt, U8 finalize = 0, DBStCbk callback = NULL, void *data = NULL, Arena *arena = NULL);
void db_close();
