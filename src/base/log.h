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

#define mscbl_log_error(module, fmt, ...) printf(ANSI_RED "[" #module "]" ANSI_PINK "[" \
                                                          "%s"                          \
                                                          "] " ANSI_RESET fmt "\n",     \
                                                 __func__, __VA_ARGS__)
#define mscbl_log_warn(module, fmt, ...) printf(ANSI_YELLOW "[" #module "]" ANSI_PINK "[" \
                                                            "%s"                          \
                                                            "] " ANSI_RESET fmt "\n",     \
                                                __func__, __VA_ARGS__)
#define mscbl_log_dbg(module, fmt, ...) printf(ANSI_CYAN "[" #module "]" ANSI_PINK "[" \
                                                         "%s"                          \
                                                         "] " ANSI_RESET fmt "\n",     \
                                               __func__, __VA_ARGS__)
#define mscbl_log(fmt, ...) printf(ANSI_CYAN "[" APP_NAME "]" ANSI_PINK "[" \
                                             "%s"                           \
                                             "] " ANSI_RESET fmt "\n",      \
                                   __func__, ##__VA_ARGS__)

#define PERF_BEGIN(A) U64 perf_start_##A = clock()
#define PERF_END(A)   printf(ANSI_GREEN "[PERF] " ANSI_BOLD #A ANSI_RESET ": %.3fms\n", (float)(clock() - perf_start_##A) / CLOCKS_PER_SEC * 1000)
