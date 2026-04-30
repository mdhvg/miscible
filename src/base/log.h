#pragma once
#include <stdio.h>
#include <time.h>

#include "base/base_core.h"

#define ANSI_RESET  "\x1b[0m"
#define ANSI_BOLD   "\x1b[1m"
#define ANSI_RED    "\x1b[31m"
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_CYAN   "\x1b[38;2;0;205;255m"
#define ANSI_GREEN  "\x1b[38;2;119;255;0m"
#define ANSI_PINK   "\x1b[38;2;255;0;154m"
#define ANSI_WHITE  "\x1b[37m"

global_v U64 log_build_path_len = sizeof(__FILE__) - sizeof("src/base/log.h");

#define mscbl_log_error(fmt, ...) printf(ANSI_RED "[%s]" ANSI_PINK "[%s] " ANSI_RESET fmt "\n", __func__, __FILE__ + log_build_path_len __VA_OPT__(, ) __VA_ARGS__)
#define mscbl_log_warn(fmt, ...)  printf(ANSI_YELLOW "[%s]" ANSI_PINK "[%s] " ANSI_RESET fmt "\n", __func__, __FILE__ + log_build_path_len __VA_OPT__(, ) __VA_ARGS__)
#define mscbl_log_dbg(fmt, ...)   printf(ANSI_CYAN "[%s]" ANSI_PINK "[%s] " ANSI_RESET fmt "\n", __func__, __FILE__ + log_build_path_len __VA_OPT__(, ) __VA_ARGS__)
#define mscbl_log(fmt, ...)       printf(ANSI_CYAN "[%s]" ANSI_PINK "[%s] " ANSI_RESET fmt "\n", __func__, __FILE__ + log_build_path_len __VA_OPT__(, ) __VA_ARGS__)

#define PERF_BEGIN(A) U64 perf_start_##A = clock()
#define PERF_END(A)   printf(ANSI_GREEN "[perf] " ANSI_BOLD #A ANSI_RESET ": %.3fms\n", (float)(clock() - perf_start_##A) / CLOCKS_PER_SEC * 1000)

#define Assert(x, message, ...)                                                        \
    do                                                                                 \
    {                                                                                  \
        if (!(x))                                                                      \
        {                                                                              \
            mscbl_log_error("(" Stringify(x) ") " message __VA_OPT__(, ) __VA_ARGS__); \
            TRAP();                                                                    \
        }                                                                              \
    } while (0)
