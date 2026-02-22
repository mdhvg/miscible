#include <condition_variable>
#include "sqlite3.h"

#include "base/base_core.h"
#include "db/db_helpers.h"

local_v sqlite3 *dbP = NULL;
local_v std::mutex db_mutex;
local_v std::condition_variable db_cv;
local_v bool db_initialized = false;

extern int init_sqlite(sqlite3 *db, char **error_message, sqlite3_api_routines const *api);

DB_CALLBACK(get_count)
{
    U64 *count = (U64 *)data;
    *count     = strtoull(argv[0], NULL, 10);
    return 0;
}

DB_STMT_CBK(get_count)
{
    *(U64 *)data = sqlite3_column_int64(stmt, 0);
}

U64 get_count(const char *query)
{
    sqlite3_stmt *stmt = db_prepare(query);
    U64 result         = 0;
    db_run_stmt(stmt, 1, get_count, &result);
    return result;
}

void db_close()
{
    std::lock_guard<std::mutex> lock(db_mutex);
    if (dbP)
    {
        Assert(sqlite3_close(dbP) == SQLITE_OK);
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
        mscbl_log_error(sqlite3, "%s\n%s\n", (err ? err : "Unknown error"), command);
        Assert(false);
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
    const char *err    = NULL;
    if (sqlite3_prepare_v2(dbP, sql, -1, &stmt, &err) != SQLITE_OK)
    {
        mscbl_log_error(sqlite3, "%s\n%s\n", (err ? err : "Unknown error"), sql);
        Assert(false);
        sqlite3_free((void *)err);
        return NULL;
    }
    return stmt;
}

U64 db_run_stmt(sqlite3_stmt *stmt, U8 finalize, db_stmt_callback callback, void *data)
{
    std::unique_lock<std::mutex> lock(db_mutex);
    db_cv.wait(lock, []() { return db_initialized; });

    U64 rows = 0;
    S32 ret;
    while ((ret = sqlite3_step(stmt)) == SQLITE_ROW)
    {
        if (callback) callback(stmt, data);
        rows++;
    }
    if (ret != SQLITE_DONE)
    {
        mscbl_log_error(sqlite3, "%s\n", sqlite3_errmsg(dbP));
        Assert(0);
    }
    if (finalize)
        sqlite3_finalize(stmt);

    return rows;
}

void db_init(const char *filename, const char *command)
{
    std::unique_lock<std::mutex> lock(db_mutex);
    Assert(sqlite3_open(filename, &dbP) == SQLITE_OK && "Couldn't load database");
    char *err = NULL;
    if (init_sqlite(dbP, &err, NULL) != SQLITE_OK)
    {
        mscbl_log_error(sqlite3ext, "%s\n", err);
    }
    _db_run_bypass(command, true);
    db_initialized = true;
    db_cv.notify_all();
}

// TODO: Can make a config parser and have db path come from it
void db_make()
{
    db_init(DB_PATH, R"(
		CREATE TABLE IF NOT EXISTS Images (
			id          INTEGER     PRIMARY KEY             AUTOINCREMENT,
			path        TEXT        UNIQUE NOT NULL,
			filename    TEXT               NOT NULL,
			atlas_id    INTEGER     REFERENCES ATLAS(id)    DEFAULT NULL,
			atlas_idx   INTEGER                             DEFAULT NULL,
			size        INTEGER                             DEFAULT 0,
			mtime       INTEGER                             DEFAULT 0,
			ctime       INTEGER                             DEFAULT 0,
			width       INTEGER                             DEFAULT 0,
			height      INTEGER                             DEFAULT 0,
			channels    INTEGER                             DEFAULT 0,
			embedding	BLOB 	                            DEFAULT NULL,
            root_dir    INTEGER     REFERENCES Dirs(id)     DEFAULT NULL,
            parent_dir  INTEGER     REFERENCES Dirs(id)     DEFAULT NULL,
            modified    INTEGER                             DEFAULT 0
		);

		CREATE INDEX IF NOT EXISTS idx_imagepath ON Images(path);

		CREATE TABLE IF NOT EXISTS Dirs (
			id          INTEGER PRIMARY KEY         AUTOINCREMENT,
			path        TEXT    UNIQUE  NOT NULL,
            mtime       INTEGER                     DEFAULT 0,
            root_dir    INTEGER REFERENCES Dirs(id) DEFAULT NULL,
            parent_dir  INTEGER REFERENCES Dirs(id) DEFAULT NULL
		);

		CREATE TABLE IF NOT EXISTS Atlas (
			id            INTEGER PRIMARY KEY AUTOINCREMENT,
			atlas_path    TEXT    NOT NULL    UNIQUE,
			image_count   INTEGER NOT NULL    DEFAULT 0
		);

		CREATE TABLE IF NOT EXISTS Holes (
			id            INTEGER PRIMARY KEY AUTOINCREMENT,
			atlas_id      INTEGER NOT NULL UNIQUE,
			atlas_index   INTEGER NOT NULL
		);
	)");
}
