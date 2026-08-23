// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "base/log.h"
#include "os/os_inc.h"
#include "base/array.h"
#include "base/string.h"
#include "app/miscible.h"

global_v FileHandle log_file = 0;

typedef struct LogEpoch LogEpoch;
struct LogEpoch
{
    U64 ticks_per_sec;
    Time base_time;
    U64 base_ticks;
};

global_v LogEpoch epoch = {0};

void mscbl_log_init(U64 log_age)
{
    String root = os_env_var(LOG_BASE_ENV, app_arena);
    String append = sv(LOG_APPEND);

    StringBuilder base = string_init(app_arena, root);
    path_join(&base, append);

    Result res = ResultSuccess();

    if (os_path_exists(StringCast(base), &res))
    {
        ArenaScoped(app_arena)
        {
            String pattern_part = sv(LOG_FILE_NAME_PRE "*" LOG_FILE_NAME_EXT);

            StringBuilder pattern = string_init(app_arena, root);
            path_join(&pattern, append);
            path_join(&pattern, pattern_part);

            U64 cur_time = os_get_timestamp();
            FileMTime *files = os_list_by_pattern(StringCast(pattern), StringCast(base), app_arena);
            for (S64 i = 0; i < arr_getsize(files); i++)
            {
                FileMTime entry = files[i];
                U64 seconds_since = cur_time - entry.mtime;
                if (log_age * 86400 <= seconds_since)
                {
                    os_file_delete(entry.path, &res);
                }
            }
        }
    }
    else
    {
        os_mkdirs(StringCast(base));
    }

    ArenaScoped(app_arena)
    {
        StringBuilder log_filepath = string_empty(app_arena, KB(4));
        Time today = os_get_localtime();

        string_format(&log_filepath, "%.*s" LOG_FILE_NAME_PRE "%02d-%02d-%04d" LOG_FILE_NAME_EXT,
                      StringSpr(base), today.date, today.month + 1, today.year);
        log_file = os_file_open(StringCast(log_filepath), FileAccess_Append, FileMode_OpenAlways, &res);
    }

    epoch = {
        .ticks_per_sec = os_get_ticks_freq(),
        .base_time = os_get_localtime(),
        .base_ticks = os_get_ticks_now(),
    };
}

void mscbl_log_deinit()
{
    Result res = ResultSuccess();
    os_file_close(log_file, &res);
}

Time get_cur_time()
{
    if (epoch.ticks_per_sec == 0)
    {
        epoch.ticks_per_sec = os_get_ticks_freq();
        epoch.base_time = os_get_localtime();
        epoch.base_ticks = os_get_ticks_now();

        // Safety fallback just in case the OS return value is somehow 0
        if (epoch.ticks_per_sec == 0)
        {
            epoch.ticks_per_sec = 1000;
        }
    }

    U64 elapsed_ticks = os_get_ticks_now() - epoch.base_ticks;
    U64 elapsed_ms = ((F64)elapsed_ticks * 1000.0) / epoch.ticks_per_sec;
    Time result = epoch.base_time;

    U64 total_ms = result.milsec + (U32)(elapsed_ms % 1000);
    result.milsec = (total_ms % 1000);
    U32 carry_seconds = (U32)(elapsed_ms / 1000) + (total_ms / 1000);

    if (carry_seconds > 0)
    {
        U32 total_seconds = result.second + carry_seconds;
        result.second = (U32)(total_seconds % 60);
        U32 carry_minutes = total_seconds / 60;

        if (carry_minutes > 0)
        {
            U32 total_minutes = result.minute + carry_minutes;
            result.minute = (U32)(total_minutes % 60);
            U32 carry_hours = total_minutes / 60;

            if (carry_hours > 0)
            {
                U32 total_hours = result.hour + carry_hours;
                result.hour = (U32)(total_hours % 24);
            }
        }
    }

    return result;
}

void mscbl_log(const char *fmt, ...)
{
    char buffer[KB(8)];

    Result res = ResultSuccess();
    Time time = get_cur_time();

    U64 header_size = snprintf(buffer, 256, "[%02d-%02d-%04d %02d:%02d:%02d.%03d] ", time.date, time.month + 1, time.year, time.hour, time.minute, time.second, time.milsec);

    va_list args;
    va_start(args, fmt);
    U64 size = vsnprintf(buffer + header_size, sizeof(buffer) - header_size, fmt, args) + header_size;
    va_end(args);

    os_file_write(log_file, size, (U8 *)buffer, &res);
}

void mscbl_log_stack(const char *fmt, const char *file)
{
    char location[KB(1)];
    snprintf(location, KB(1), fmt, file);

    const S32 max_frames = 128;
    void *stack[128];
    U32 frame_count = os_get_frames(2, max_frames, stack);

    mscbl_log_bare("ERROR", location, "---------- STACK TRACE ----------");
    for (U32 frame = 0; frame < frame_count; frame++)
    {
        char buffer[KB(1)];
        os_get_frame_text(buffer, stack[frame]);
        mscbl_log_bare("ERROR", location, "%s", buffer);
    }
    mscbl_log_bare("ERROR", location, "---------------------------------");
}

void _mscbl_log_bare(const char *level, const char *location, const char *fmt, ...)
{
    char buffer[KB(8)];

    Result res = ResultSuccess();
    Time time = get_cur_time();

    U64 header_size = snprintf(buffer, 256, "[%02d-%02d-%04d %02d:%02d:%02d.%03d] ", time.date, time.month + 1, time.year, time.hour, time.minute, time.second, time.milsec);

    U64 prefix_size = snprintf(buffer + header_size, 512, "[%s] [%s] ", level, location);
    U64 offset = header_size + prefix_size;

    va_list args;
    va_start(args, fmt);
    U64 payload_size = vsnprintf(buffer + offset, sizeof(buffer) - offset, fmt, args);
    va_end(args);

    U64 total_size = offset + payload_size;

    os_file_write(log_file, total_size, (U8 *)buffer, &res);
}
