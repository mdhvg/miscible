#pragma once
#if defined(__cplusplus)
#define _Bool bool
#endif

// Define required positional arguments
// #define REQUIRED_ARGS \
//     REQUIRED_STRING_ARG(input_file, "input", "Input file path") \
//     REQUIRED_STRING_ARG(output_file, "output", "Output file path")

// Define optional arguments with defaults
// #define OPTIONAL_ARGS \
//     OPTIONAL_UINT_ARG(threads, 1, "-t", "threads", "Number of threads to use")

// Define boolean flags
#define BOOLEAN_ARGS                                                        \
    BOOLEAN_ARG(noembed, "--no-embed", "Disable embedding (For debugging)") \
    BOOLEAN_ARG(help, "--help", "Print help")

#include "easyargs.h"

global_v args_t cli_args;
