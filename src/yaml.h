// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include "libfyaml.h"

#include "base/string.h"

String _yaml_scan_string(Arena *arena, fy_node *root, const char *node_selector, const char *text_selector);
#define yaml_scan_string(arena, root, sel) _yaml_scan_string(arena, root, sel, sel " %4096s")

U64 _yaml_scan_int(fy_node *root, const char *text_selector);
#define yaml_scan_int(root, sel) _yaml_scan_int(root, sel " %zu")

F32 _yaml_scan_float(fy_node *root, const char *text_selector);
#define yaml_scan_float(root, sel) _yaml_scan_float(root, sel " %f")

void yaml_scan_hash(fy_node *root, U8 *hash_buf, U64 digest_size, const char *text_selector);
