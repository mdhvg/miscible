#pragma once
#include <time.h>
#include <stdio.h>

#include "base/base_core.h"

#define ANSI_RESET "\x1b[0m"
#define ANSI_CYAN  "\x1b[38;2;0;205;255m"
#define ANSI_GREEN "\x1b[38;2;119;255;0m"

global_v U64 log_build_path_len = sizeof(__FILE__) - sizeof("src/base/log.h");

#define mscbl_log_info(fmt, ...)  _mscbl_log("INFO", fmt, __VA_ARGS__)
#define mscbl_log_warn(fmt, ...)  _mscbl_log("WARN", fmt, __VA_ARGS__)
#define mscbl_log_error(fmt, ...) _mscbl_log("ERROR", fmt, __VA_ARGS__)

#define PERF_BEGIN(A) U64 __perf_start_##A = clock()
#define PERF_END(A)   mscbl_log_info("[perf]: %.3fms", (F32)(clock() - __perf_start_##A) / CLOCKS_PER_SEC * 1000)

#define Assert(x, message, ...)                                                        \
    do                                                                                 \
    {                                                                                  \
        if (!(x))                                                                      \
        {                                                                              \
            mscbl_log_error("(" Stringify(x) ") " message __VA_OPT__(, ) __VA_ARGS__); \
            TRAP();                                                                    \
        }                                                                              \
    } while (0)

void mscbl_log_init();
void mscbl_log_deinit();
MSCBL_API void mscbl_log(const char *fmt, ...);
#define _mscbl_log(level, fmt, ...) mscbl_log("[" level "] [%s:" Stringify(__LINE__) "] " fmt "\n", __FILE__ + log_build_path_len __VA_OPT__(, ) __VA_ARGS__)
