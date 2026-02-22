#pragma once
#include <stdio.h>
#include <time.h>
#include "base/base_core.h"

#define ASCII_RESET  "\x1b[0m"
#define ASCII_RED    "\x1b[31m"
#define ASCII_YELLOW "\x1b[33m"
#define ASCII_PURPLE "\x1b[35m"
#define ASCII_WHITE  "\x1b[37m"

#define mscbl_log_error(module, fmt, ...) printf(ASCII_RED "[" #module "]" ASCII_RESET ASCII_PURPLE "[" \
                                                           "%s"                                         \
                                                           "] " ASCII_RESET fmt "\n",                   \
                                                 __func__, __VA_ARGS__)
#define mscbl_log_warn(module, fmt, ...) printf(ASCII_YELLOW "[" #module "] " ASCII_RESET fmt "\n", __VA_ARGS__)
#define mscbl_log_dbg(module, fmt, ...)  printf(ASCII_WHITE "[" #module "] " ASCII_RESET fmt "\n", __VA_ARGS__)
#define mscbl_log(fmt, ...)              printf(ASCII_WHITE "[" APP_NAME "] " ASCII_RESET fmt "\n", ##__VA_ARGS__)

#define PERF_BEGIN(A) U64 perf_start_##A = clock()
#define PERF_END(A)   printf("[PERF (" #A ")]: %.3fms\n", (float)(clock() - perf_start_##A) / CLOCKS_PER_SEC * 1000)
