// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include <time.h>
#include <stdio.h>

#include "base/base_core.h"

#define ANSI_RESET  "\x1b[0m"
#define ANSI_CYAN   "\x1b[38;2;0;205;255m"
#define ANSI_GREEN  "\x1b[38;2;119;255;0m"
#define ANSI_RED    "\x1b[31m"
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_PURPLE "\x1b[38;2;166;78;227m"

global_v U64 log_build_path_len = sizeof(__FILE__) - sizeof("src/base/log.h");

#if DBG
#define _color_log(c1, fmt, ...)             printf(c1 "[%s:" Stringify(__LINE__) "] " ANSI_RESET fmt "\n", __FILE__ + (sizeof(__FILE__) > log_build_path_len ? log_build_path_len : 0) __VA_OPT__(, ) __VA_ARGS__)
#define mscbl_log_error(fmt, ...)            _color_log(ANSI_RED, fmt, __VA_ARGS__)
#define mscbl_log_warn(fmt, ...)             _color_log(ANSI_YELLOW, fmt, __VA_ARGS__)
#define mscbl_log_info(fmt, ...)             _color_log(ANSI_CYAN, fmt, __VA_ARGS__)
#define mscbl_log_bare(level, loc, fmt, ...) printf(ANSI_PURPLE "[%s] [%s] " ANSI_RESET fmt "\n", level, loc __VA_OPT__(, ) __VA_ARGS__)
#else
#define mscbl_log_info(fmt, ...)             _mscbl_log("INFO", fmt, __VA_ARGS__)
#define mscbl_log_warn(fmt, ...)             _mscbl_log("WARN", fmt, __VA_ARGS__)
#define mscbl_log_error(fmt, ...)            _mscbl_log("ERROR", fmt, __VA_ARGS__)
#define mscbl_log_bare(level, loc, fmt, ...) _mscbl_log_bare(level, loc, fmt "\n" __VA_OPT__(, ) __VA_ARGS__)
#endif

#define perf_beg(A) U64 __perf_start_##A = clock()
#define perf_gap(A) clock() - __perf_start_##A
#define perf_end(A) mscbl_log_info("[perf]: %.4fms", (F64)(clock() - __perf_start_##A) / CLOCKS_PER_SEC * 1000.0f)

#define Assert(x, message, ...)                                                               \
    do                                                                                        \
    {                                                                                         \
        if (!(x))                                                                             \
        {                                                                                     \
            mscbl_log_error("(" Stringify(x) ") " message __VA_OPT__(, ) __VA_ARGS__);        \
            mscbl_log_stack(                                                                  \
                "%s:" Stringify(__LINE__),                                                    \
                __FILE__ + (sizeof(__FILE__) > log_build_path_len ? log_build_path_len : 0)); \
            TRAP();                                                                           \
        }                                                                                     \
    } while (0)

void mscbl_log_init(U64 log_age);
void mscbl_log_deinit();
MSCBL_API void mscbl_log(const char *fmt, ...);
#define _mscbl_log(level, fmt, ...) mscbl_log("[" level "] [%s:" Stringify(__LINE__) "] " fmt "\n", __FILE__ + (sizeof(__FILE__) > log_build_path_len ? log_build_path_len : 0) __VA_OPT__(, ) __VA_ARGS__)
MSCBL_API void mscbl_log_stack(const char *fmt, const char *file);
MSCBL_API void _mscbl_log_bare(const char *level, const char *location, const char *fmt, ...);
