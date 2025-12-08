#pragma once

#include "sqlite3.h"

#include "base/string.h"

#define DB_CALLBACK(name) int name(void *data, int argc, char **argv, char **column_name)

void db_init(const char *filename, const char *command);
void db_make(void *);
void db_run(const char *command, sqlite3_callback callback = nullptr, void *data = nullptr);
inline void db_run(String command, sqlite3_callback callback = NULL, void *data = NULL)
{
	db_run(str_to_cstr(command), callback, data);
}
void db_close();