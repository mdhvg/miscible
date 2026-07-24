// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include "sqlite3.h"

#include "base/string.h"

#define DB_CALLBACK(name) int name(void *data, int argc, char **argv, char **column_name)
#define DBStmtCbk(name)   void name(sqlite3_stmt *stmt, void *data, Arena *arena)

DB_CALLBACK(get_count);
DBStmtCbk(get_count);
DBStmtCbk(get_id);
U64 get_count(const char *query);

typedef DBStmtCbk(DBStCbk);

void db_init(const char *filename, const char *command);
void db_make();
void db_run(const char *command, sqlite3_callback callback = nullptr, void *data = nullptr);
inline void db_run(String command, sqlite3_callback callback = NULL, void *data = NULL)
{
    db_run(CStrCast(command), callback, data);
}
MSCBL_API sqlite3_stmt *db_prepare(const char *sql);
MSCBL_API U64 db_run_stmt(sqlite3_stmt *stmt, U8 finalize = 0, DBStCbk callback = NULL, void *data = NULL, Arena *arena = NULL);
void db_close();
