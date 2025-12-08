#include <mutex>

#include "base/core.h"
#include "db/db_helpers.h"

internal sqlite3 *dbP = NULL;
internal std::mutex db_mutex;
internal std::condition_variable db_cv;
internal bool db_initialized = false;

void db_close()
{
	std::lock_guard<std::mutex> lock(db_mutex);
	if (dbP)
	{
		Assert(sqlite3_close(dbP) == SQLITE_OK && "Failed to close db");
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

	char *errorMessage = nullptr;
	int result = sqlite3_exec(dbP, command, callback, data, &errorMessage);
	if (result != SQLITE_OK)
	{
		printf("[SQLite error]\nQuery:\n%s\nCode: %d, Message: %s\n", command, result,
			   (errorMessage ? errorMessage : "Unknown error"));
		Assert(false);
	}
	if (errorMessage)
	{
		sqlite3_free(errorMessage);
	}
}

void db_run(const char *command, sqlite3_callback callback, void *data)
{
	_db_run_bypass(command, false, callback, data);
}

void db_init(const char *filename, const char *command)
{
	std::unique_lock<std::mutex> lock(db_mutex);
	Assert(sqlite3_open(filename, &dbP) == SQLITE_OK && "Couldn't load database");
	_db_run_bypass(command, true);
	db_initialized = true;
	db_cv.notify_all();
}

// TODO: Make an entry for mtime, ctime (UNIX epoch both), size and other stuff
// in Images table.
// TODO: Can make a config parser and have db path come from it
// NOTE: Embedding is an integer in Images table being used as a bool
void db_make(void *)
{
	db_init(DB_PATH, R"(
		CREATE TABLE IF NOT EXISTS Images (
			id          INTEGER PRIMARY KEY AUTOINCREMENT,
			path        TEXT    UNIQUE  NOT NULL,
			filename    TEXT            NOT NULL,
			atlas_id    INTEGER DEFAULT NULL,
			atlas_idx   INTEGER DEFAULT NULL,
			size        INTEGET DEFAULT 0,
			mtime       INTEGER DEFAULT 0,
			ctime       INTEGER DEFAULT 0,
			width       INTEGER DEFAULT 0,
			height      INTEGER DEFAULT 0,
			channels    INTEGER DEFAULT 0,
			embedding   INTEGER DEFAULT NULL
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
