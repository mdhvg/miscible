#include "base/string.h"
#include "db/db_helpers.h"
#include "base/stack.h"
#include "base/tree.h"
#include "base/path.h"
#include "scan.h"
#include "os/os_inc.h"
#include "sqlite3.h"
#include <linux/stat.h>

#if OS_LINUX
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

struct PathNode
{
    Path path;
    U64 depth;
    U64 scanned;
};

#define PUSH_LIMIT 500
#define MAX_DEPTH  15

Stack(path_stack, PathNode) = {0};
Stack(cur_stack, PathNode)  = {0};

void push_paths(sqlite3_stmt *stmt, void *)
{
    PathNode p = {s_cpy(scan_scratch, (const char *)sqlite3_column_text(stmt, 0))};
    stack_push(path_stack, p);
}

void get_id(sqlite3_stmt *stmt, void *data)
{
    U64 *id = (U64 *)data;
    *id     = sqlite3_column_int64(stmt, 0);
}

B32 entry_exists(String path, const char *filename)
{
    sqlite3_stmt *stmt = db_prepare("SELECT id FROM Images WHERE path = ? || ?;");
    sqlite3_bind_text(stmt, 1, (const char *)path.v, path.size, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, filename, 0, SQLITE_STATIC);
    return db_run_stmt(stmt, 1);
}

#if OS_WINDOWS
// NOTE: Yes, my OCD also triggers looking at this (not clean) code. But, I
// honestly can't find a better way to do it. If you see this and can do it
// better (while keeping the constraints), please feel free to do so
THREAD_FUNC(scan_dirs)
{
    Temp scan_temp = temp_begin(scan_scratch);

    path_stack = stack_init(scan_temp.arena, 1000, PathNode);
    cur_stack  = stack_init(scan_temp.arena, 800, PathNode);

    sqlite3_stmt *stmt = db_prepare("SELECT path from Dirs;");
    db_run_stmt(stmt, 1, push_paths);

    WIN32_FIND_DATAW fdFile;
    HANDLE hFind = NULL;

    U64 file_count = 0;
    U64 abs_total  = 0;
    while (!stack_empty(path_stack))
    {
        PathNode *cur     = stack_front(path_stack);
        String expression = string_format(scan_temp.arena, "%.*s/*.*", cur->path.size, cur->path.v);

        file_count   = 0;
        B32 has_next = 0;
        for (hFind = FindFirstFileW(str_to_wcstr(scan_temp.arena, expression), &fdFile);
             (has_next = FindNextFileW(hFind, &fdFile)) && file_count < cur->scanned + PUSH_LIMIT;)
        {
            mscbl_log("File: %ls", fdFile.cFileName);
            if (hFind == INVALID_HANDLE_VALUE)
            {
                stack_pop(path_stack);
                break;
            }
            if (match_front(fdFile.cFileName, L".") ||
                match_front(fdFile.cFileName, L".."))
            {
                continue;
            }
            if (fdFile.dwFileAttributes ^ FILE_ATTRIBUTE_DIRECTORY)
            {
                // if (entry_exists(cur->path, fdFile.cFileName))
                // {
                // 	continue;
                // }
                // TODO: Find attribute combinatin to locate all image files safely (without falling for symlinks, system files, etc)
                // if (fdFile.dwFileAttributes & FILE_ATTRIBUTE_NORMAL)
                // TODO: Check filetype using magic bytes
                if (match_end(fdFile.cFileName, L".jpg") || match_end(fdFile.cFileName, L".png"))
                {
                    FILETIME write_time = fdFile.ftLastWriteTime;
                    FILETIME make_time  = fdFile.ftCreationTime;
                    U64 mtime = 0, ctime = 0, size = 0;
                    mtime = write_time.dwHighDateTime;
                    mtime <<= 32;
                    mtime |= write_time.dwLowDateTime;
                    ctime = make_time.dwHighDateTime;
                    ctime <<= 32;
                    ctime |= make_time.dwLowDateTime;
                    size = fdFile.nFileSizeHigh;
                    size <<= 32;
                    size |= fdFile.nFileSizeLow;
                    Assert(cur->path.v[0]);
                    sqlite3_stmt *stmt = db_prepare(R"(INSERT INTO Images(path, filename, size, mtime, ctime) VALUES(? || '/' || ?, ?, ?, ?, ?) ON CONFLICT(path) DO NOTHING RETURNING id;)");
                    sqlite3_bind_text(stmt, 1, (const char *)cur->path.v, cur->path.size, SQLITE_STATIC);
                    sqlite3_bind_text16(stmt, 2, fdFile.cFileName, -1, SQLITE_STATIC);
                    sqlite3_bind_text16(stmt, 3, fdFile.cFileName, -1, SQLITE_STATIC);
                    sqlite3_bind_int64(stmt, 4, size);
                    sqlite3_bind_int64(stmt, 5, mtime);
                    sqlite3_bind_int64(stmt, 6, ctime);

                    U64 id = 0;
                    if (db_run_stmt(stmt, 1, get_id, &id) > 0)
                    {
                        Image img     = {id};
                        Image_Node *n = tree_node(mscbl.persistent_arena, img, Image);
                        tree_push(&ui_state.images, n, Image_cmp, Image);
                    }
                }
                abs_total++;
            }
            if (fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && cur->depth + 1 < MAX_DEPTH)
            {
                PathNode new_dir = {string_format(scan_temp.arena, "%.*s/%ls", cur->path.size, (char *)cur->path.v, &fdFile.cFileName), cur->depth + 1};
                stack_push(cur_stack, new_dir);
                file_count++;
                continue;
            }
        }
        if (has_next)
        {
            cur->scanned = file_count;
        }
        else
        {
            stack_pop(path_stack);
        }
        while (!stack_empty(cur_stack)) { stack_push(path_stack, stack_pop(cur_stack)); }
        FindClose(hFind);
    }
    temp_end(scan_temp);
    mscbl_log("Absolute total: %llu\n", abs_total);
}
#elif OS_LINUX
THREAD_FUNC(scan_dirs)
{
    Temp scan_temp = temp_begin(scan_scratch);

    path_stack = stack_init(scan_temp.arena, 1000, PathNode);
    cur_stack  = stack_init(scan_temp.arena, 800, PathNode);

    sqlite3_stmt *stmt = db_prepare("SELECT path from Dirs;");
    db_run_stmt(stmt, 1, push_paths);

    S32 dirfd;
    DIR *dir;
    dirent64 *entry;
    struct statx stat;

    U64 file_count = 0;
    U64 abs_total  = 0;
    while (!stack_empty(path_stack))
    {
        PathNode *cur = stack_front(path_stack);
        dirfd         = open(str_to_cstr(cur->path), O_RDONLY | O_DIRECTORY);
        Assert(dirfd >= 0);
        DIR *dir = fdopendir(dirfd);

        if (!dir)
        {
            stack_pop(path_stack);
            break;
        }

        file_count   = 0;
        B32 has_next = 1;
        for (entry = readdir64(dir); has_next && file_count < cur->scanned + PUSH_LIMIT; (has_next = ((entry = readdir64(dir)) != NULL)))
        {
            if (match_front(entry->d_name, ".") ||
                match_front(entry->d_name, ".."))
            {
                continue;
            }

            mscbl_log("%*s%s", cur->depth * 2, "", entry->d_name);

            if (statx(dirfd, entry->d_name, AT_SYMLINK_NOFOLLOW, STATX_TYPE | STATX_SIZE | STATX_MTIME | STATX_BTIME, &stat) == -1)
            {
                continue;
            }

            if (S_ISREG(stat.stx_mode))
            {
                // TODO: Check filetype using magic bytes
                if (match_end(entry->d_name, ".jpg") || match_end(entry->d_name, ".png"))
                {
                    abs_total++;
                    U64 mtime = stat.stx_mtime.tv_sec;
                    U64 btime = stat.stx_btime.tv_sec;
                    U64 size  = stat.stx_size;
                    Assert(cur->path.v[0]);
                    sqlite3_stmt *stmt = db_prepare(R"(INSERT INTO Images(path, filename, size, mtime, ctime) VALUES(? || '/' || ?, ?, ?, ?, ?) ON CONFLICT(path) DO NOTHING RETURNING id;)");
                    sqlite3_bind_text(stmt, 1, (const char *)cur->path.v, cur->path.size, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 2, entry->d_name, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 3, entry->d_name, -1, SQLITE_STATIC);
                    sqlite3_bind_int64(stmt, 4, size);
                    sqlite3_bind_int64(stmt, 5, mtime);
                    sqlite3_bind_int64(stmt, 6, btime);

                    U64 id = 0;
                    if (db_run_stmt(stmt, 1, get_id, &id) > 0)
                    {
                        Image img     = {id};
                        Image_Node *n = tree_node(mscbl.persistent_arena, img, Image);
                        tree_push(&ui_state.images, n, Image_cmp, Image);
                    }
                }
            }
            if (S_ISDIR(stat.stx_mode) && cur->depth + 1 < MAX_DEPTH)
            {
                PathNode new_dir = {string_format(scan_temp.arena, "%.*s/%s", cur->path.size, (char *)cur->path.v, entry->d_name), cur->depth + 1};
                stack_push(cur_stack, new_dir);
                file_count++;
                continue;
            }
        }
        if (has_next)
        {
            cur->scanned = file_count;
        }
        else
        {
            stack_pop(path_stack);
        }
        while (!stack_empty(cur_stack)) { stack_push(path_stack, stack_pop(cur_stack)); }

        close(dirfd);
        closedir(dir);
    }
    temp_end(scan_temp);
    mscbl_log("Absolute total: %zu\n", abs_total);
}
#endif
