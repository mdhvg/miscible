#include <condition_variable>
#include "sqlite3.h"

#include "base/base_core.h"
#include "base/log.h"
#include "db/db_helpers.h"
#include "config.h"

local_v sqlite3 *dbP = NULL;
local_v std::mutex db_mutex;
local_v std::condition_variable db_cv;
local_v bool db_initialized = false;

extern int init_sqlite(sqlite3 *db, char **error_message, sqlite3_api_routines const *api);

DB_CALLBACK(get_count)
{
    U64 *count = (U64 *)data;
    *count = strtoull(argv[0], NULL, 10);
    return 0;
}

DBStmtCbk(get_count)
{
    *(U64 *)data = sqlite3_column_int64(stmt, 0);
}

DBStmtCbk(get_id)
{
    *(S64 *)data = sqlite3_column_int64(stmt, 0);
}

U64 get_count(const char *query)
{
    sqlite3_stmt *stmt = db_prepare(query);
    U64 result = 0;
    db_run_stmt(stmt, 1, get_count, &result);
    return result;
}

void db_close()
{
    std::lock_guard<std::mutex> lock(db_mutex);
    if (dbP)
    {
        Assert(sqlite3_close(dbP) == SQLITE_OK, "failed to close db");
        dbP = NULL;
    }
}

void _db_run_bypass(const char *command, bool bypass = false, sqlite3_callback callback = NULL, void *data = NULL)
{
    if (!bypass)
    {
        std::unique_lock<std::mutex> lock(db_mutex);
        db_cv.wait(lock, []() { return db_initialized; });
    }

    char *err = NULL;
    if (sqlite3_exec(dbP, command, callback, data, &err) != SQLITE_OK)
    {
        Assert(0, "%s\n%s", (err ? err : "Unknown error"), command);
        sqlite3_free(err);
    }
}

void db_run(const char *command, sqlite3_callback callback, void *data)
{
    _db_run_bypass(command, false, callback, data);
}

sqlite3_stmt *db_prepare(const char *sql)
{
    std::unique_lock<std::mutex> lock(db_mutex);
    db_cv.wait(lock, []() { return db_initialized; });

    sqlite3_stmt *stmt = NULL;
    const char *err = NULL;
    if (sqlite3_prepare_v2(dbP, sql, -1, &stmt, &err) != SQLITE_OK)
    {
        Assert(0, "%s\n%s", (err ? err : "Unknown error"), sql);
        sqlite3_free((void *)err);
    }
    return stmt;
}

U64 db_run_stmt(sqlite3_stmt *stmt, U8 finalize, DBStCbk callback, void *data, Arena *arena)
{
    std::unique_lock<std::mutex> lock(db_mutex);
    db_cv.wait(lock, []() { return db_initialized; });

    U64 rows = 0;
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

void db_init(String path, const char *command)
{
    std::unique_lock<std::mutex> lock(db_mutex);
    Assert(sqlite3_open(CStrCast(path), &dbP) == SQLITE_OK, "Couldn't load database");
    char *err = NULL;
    Assert(init_sqlite(dbP, &err, NULL) == SQLITE_OK, "%s", err);
    _db_run_bypass(command, true);
    // sqlite3_trace_v2(dbP, SQLITE_TRACE_STMT, trace_callback, NULL);
    db_initialized = true;
    db_cv.notify_all();
}

void db_make()
{
    int x = 0;
    db_init(mscbl_config.db_path,
            (
#include "db/init.sql"
                ));
}
