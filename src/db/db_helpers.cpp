#include "base/base_core.h"
#include "sqlite3.h"
#include "db/db_helpers.h"

internal sqlite3 *dbP = NULL;
internal std::mutex db_mutex;
internal std::condition_variable db_cv;
internal bool db_initialized = false;

extern int init_sqlite(sqlite3 *db, char **error_message, sqlite3_api_routines const *api);

DB_CALLBACK(get_count)
{
	U64 *count = (U64 *)data;
	*count	   = strtoull(argv[0], NULL, 10);
	return 0;
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
		printf("[sqlite3 error]: %s\n%s\n", (err ? err : "Unknown error"), command);
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
	const char *err	   = NULL;
	if (sqlite3_prepare_v2(dbP, sql, -1, &stmt, &err) != SQLITE_OK)
	{
		printf("[sqlite3 error]: %s\n%s\n", (err ? err : "Unknown error"), sql);
		Assert(false);
		sqlite3_free((void *)err);
		return NULL;
	}
	return stmt;
}

void db_run_stmt(sqlite3_stmt *stmt, U8 finalize, db_stmt_callback callback, void *data)
{
	std::unique_lock<std::mutex> lock(db_mutex);
	db_cv.wait(lock, []() { return db_initialized; });

	S32 ret;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW)
	{
		if (callback) callback(stmt, data);
	}
	if (ret != SQLITE_DONE)
	{
		printf("[sqlite3 error]: %s\n", sqlite3_errmsg(dbP));
		Assert(0);
	}
	if (finalize)
		sqlite3_finalize(stmt);
}

void db_init(const char *filename, const char *command)
{
	std::unique_lock<std::mutex> lock(db_mutex);
	Assert(sqlite3_open(filename, &dbP) == SQLITE_OK && "Couldn't load database");
	char *err = NULL;
	if (init_sqlite(dbP, &err, NULL) != SQLITE_OK)
	{
		printf("[sqlite3ext error]: %s\n", err);
	}
	_db_run_bypass(command, true);
	db_initialized = true;
	db_cv.notify_all();
}

// TODO: Make an entry for mtime, ctime (UNIX epoch both), size and other stuff
// in Images table.
// TODO: Can make a config parser and have db path come from it
// NOTE: Embedding is an integer in Images table being used as a bool
void db_make()
{
	db_init(DB_PATH, R"(
		CREATE TABLE IF NOT EXISTS Images (
			id          INTEGER PRIMARY KEY AUTOINCREMENT,
			path        TEXT    UNIQUE  NOT NULL,
			filename    TEXT            NOT NULL,
			atlas_id    INTEGER DEFAULT NULL,
			atlas_idx   INTEGER DEFAULT NULL,
			size        INTEGER DEFAULT 0,
			mtime       INTEGER DEFAULT 0,
			ctime       INTEGER DEFAULT 0,
			width       INTEGER DEFAULT 0,
			height      INTEGER DEFAULT 0,
			channels    INTEGER DEFAULT 0,
			embedding	BLOB 	DEFAULT NULL
		);

		CREATE INDEX IF NOT EXISTS idx_imagepath ON Images(path);

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
