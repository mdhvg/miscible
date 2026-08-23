// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "sqlite3.h"

#include "db/db_helpers.h"
#include "config.h"
#include "base/log.h"

local_v sqlite3 *dbP = NULL;
local_v Mutex db_mutex = {0};

extern int init_sqlite(sqlite3 *db, char **error_message, sqlite3_api_routines const *api);

DBStmtCbk(get_count)
{
    *(U64 *)data = sqlite3_column_int64(stmt, 0);
}

DBStmtCbk(get_id)
{
    *(S64 *)data = sqlite3_column_int64(stmt, 0);
}

DBStmtCbk(get_path)
{
    *(String *)data = string_copy(arena, sqlite3_column_text(stmt, 0));
}

void db_close()
{
    DeferLoop(os_mutex_lock(&db_mutex), os_mutex_unlock(&db_mutex))
    {
        if (dbP)
        {
            Assert(sqlite3_close(dbP) == SQLITE_OK, "failed to close db");
            dbP = NULL;
        }
    }
    os_mutex_destroy(&db_mutex);
}

void _db_run_bypass(const char *command, bool bypass = false, sqlite3_callback callback = NULL, void *data = NULL)
{
    if (!bypass)
        os_mutex_lock(&db_mutex);

    char *err = NULL;
    if (sqlite3_exec(dbP, command, callback, data, &err) != SQLITE_OK)
    {
        Assert(0, "%s\n%s", (err ? err : "Unknown error"), command);
        sqlite3_free(err);
    }

    if (!bypass)
        os_mutex_unlock(&db_mutex);
}

void db_run(const char *command, sqlite3_callback callback, void *data)
{
    _db_run_bypass(command, false, callback, data);
}

sqlite3_stmt *db_prepare(const char *sql)
{
    sqlite3_stmt *stmt = NULL;
    DeferLoop(os_mutex_lock(&db_mutex), os_mutex_unlock(&db_mutex))
    {
        Assert(sqlite3_prepare_v2(dbP, sql, -1, &stmt, NULL) == SQLITE_OK, "%s\n%s", sqlite3_errmsg(dbP), sql);
    }
    return stmt;
}

U64 db_run_stmt(sqlite3_stmt *stmt, U8 finalize, DBStCbk callback, void *data, Arena *arena)
{
    U64 rows = 0;
    DeferLoop(os_mutex_lock(&db_mutex), os_mutex_unlock(&db_mutex))
    {
        S32 ret;
        while ((ret = sqlite3_step(stmt)) == SQLITE_ROW)
        {
            if (callback)
                callback(stmt, data, arena);
            rows++;
        }
        Assert(ret == SQLITE_DONE, "%s", sqlite3_errmsg(dbP));
        if (finalize)
            sqlite3_finalize(stmt);
    }

    return rows;
}

int trace_callback(unsigned int type, void *context, void *p, void *x)
{
    if (type == SQLITE_TRACE_STMT)
    {
        sqlite3_stmt *stmt = (sqlite3_stmt *)p;
        char *sql = sqlite3_expanded_sql(stmt);
        mscbl_log_info("STMT: %s", sql);
        sqlite3_free(sql);
    }

    return SQLITE_OK;
}

void db_init()
{
    os_mutex_init(&db_mutex);
    DeferLoop(os_mutex_lock(&db_mutex), os_mutex_unlock(&db_mutex))
    {
        Assert(sqlite3_open(CStrCast(mscbl_config.db_path), &dbP) == SQLITE_OK, "Couldn't load database");
        char *err = NULL;
        Assert(init_sqlite(dbP, &err, NULL) == SQLITE_OK, "%s", err);
        int x = 0;
        _db_run_bypass((
#include "db/init.sql"
                           ),
                       true);
#if DBG
        sqlite3_trace_v2(dbP, SQLITE_TRACE_STMT, trace_callback, NULL);
#endif
    }
}
