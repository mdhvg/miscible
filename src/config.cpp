// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "libfyaml/libfyaml-core.h"
#include "sha2.h"

#include "yaml.h"
#include "db/view.h"
#include "config.h"
#include "base/log.h"
#include "os/os_inc.h"
#include "base/array.h"
#include "base/string.h"

Config mscbl_config = {0};
Arena *config_arena = NULL;

RemoteFileArr config_parse_remote_file(Arena *arena, fy_node *array)
{
    RemoteFileArr files = NULL;
    fy_node *cur = NULL;
    void *pre = NULL;

    while ((cur = fy_node_sequence_iterate(array, &pre)))
    {
        RemoteFile file = {
            .size = yaml_scan_int(cur, "/Size"),
            .url = yaml_scan_string(arena, cur, "/URL"),
            .name = yaml_scan_string(arena, cur, "/Name"),
        };
        yaml_scan_hash(cur, file.hash, SHA256_DIGEST_SIZE, "/Hash");

        da_push(arena, files, file);
    }

    return files;
}

ModelGroupArr config_parse_model_groups(Arena *arena, fy_node *root)
{
    if (fy_node_sequence_is_empty(root))
        return NULL;

    fy_node *cur = NULL;
    void *pre = NULL;
    ModelGroupArr groups = NULL;
    while ((cur = fy_node_sequence_iterate(root, &pre)))
    {
        ModelGroup group = {
            .name = yaml_scan_string(arena, cur, "/Name"),
            .common_files = config_parse_remote_file(arena, fy_node_by_path(cur, "/Files", FY_NT, FYNWF_FOLLOW)),
        };

        {
            fy_node *cur1 = NULL;
            void *pre1 = NULL;

            fy_node *variants_node = fy_node_by_path(cur, "/Variants", FY_NT, FYNWF_FOLLOW);
            while ((cur1 = fy_node_sequence_iterate(variants_node, &pre1)))
            {
                ModelVariant variant = {.precision = (Precision)yaml_scan_int(cur1, "/Precision")};

                variant.text_files = config_parse_remote_file(arena, fy_node_by_path(cur1, "/Text", FY_NT, FYNWF_FOLLOW));
                variant.vision_files = config_parse_remote_file(arena, fy_node_by_path(cur1, "/Vision", FY_NT, FYNWF_FOLLOW));
                da_push(arena, group.variants, variant);
            }
        }
        da_push(arena, groups, group);
    }

    return groups;
}

/*
 * Reserved symbols
 * '~' is for user home directory (C:/Users/user on Win32 or /home/user on Unix)
 * '$' is for app data directory
 *
 * For eg. if app_data is ~/Miscible, "$/atlas" resolves to ~/Miscible/atlas
 */

Config default_config()
{
#include "config.yaml"
    String config_text = sv(__mscbl_cfg_text);

    fy_document *config_doc = fy_document_build_from_string(NULL, CStrCast(config_text), config_text.size);
    Assert(config_doc, "fyd is NULL");
    fy_node *config_root = fy_document_root(config_doc);

    Config config = {
        .app_data = yaml_scan_string(config_arena, config_root, "/AppData"),
        .atlas_dir = yaml_scan_string(config_arena, config_root, "/AtlasDir"),
        .db_path = yaml_scan_string(config_arena, config_root, "/DBPath"),

        .settings = {
            .scan_depth = yaml_scan_int(config_root, "/Settings/ScanDepth"), // To stop formatter from collapsing these
            .font_size = yaml_scan_float(config_root, "/Settings/FontSize"), // To stop formatter from collapsing these
            .log_age_days = yaml_scan_int(config_root, "/Settings/LogAge"),  // To stop formatter from collapsing these
        },

        .view_settings = {
            .sort_basis = (SortType)yaml_scan_int(config_root, "/ViewSettings/SortBasis"),         // To stop formatter from collapsing these
            .descending = ((yaml_scan_int(config_root, "/ViewSettings/Descending") == 0) ? 0 : 1), // To stop formatter from collapsing these
        },

        .inf_settings = {
            .base_dir = yaml_scan_string(config_arena, config_root, "/Inference/BaseDir"),                                         // To stop formatter from collapsing these
            .ggml = config_parse_model_groups(config_arena, fy_node_by_path(config_root, "/Inference/GGML", FY_NT, FYNWF_FOLLOW)), // To stop formatter from collapsing these
            .onnx = config_parse_model_groups(config_arena, fy_node_by_path(config_root, "/Inference/ONNX", FY_NT, FYNWF_FOLLOW)), // To stop formatter from collapsing these
        },
    };

    String backend = yaml_scan_string(config_arena, fy_node_by_path(config_root, "/Inference/Active", FY_NT, FYNWF_FOLLOW), "/Backend");
    String model_name = yaml_scan_string(config_arena, fy_node_by_path(config_root, "/Inference/Active", FY_NT, FYNWF_FOLLOW), "/Name");
    U64 model_precision = yaml_scan_int(fy_node_by_path(config_root, "/Inference/Active", FY_NT, FYNWF_FOLLOW), "/Precision");

    ModelGroupArr backend_group = NULL;
    if (!string_cmp(backend, sv("ONNX")))
    {
        config.inf_settings.active.backend = Backend_ONNX;
        backend_group = config.inf_settings.onnx;
    }
    else
    {
        config.inf_settings.active.backend = Backend_GGML;
        backend_group = config.inf_settings.ggml;
    }

    for (S64 gi = 0; gi < da_getsize(backend_group); gi++)
    {
        if (!string_cmp(backend_group[gi].name, model_name))
        {
            config.inf_settings.active.group = &backend_group[gi];
            ModelVariantArr variants = backend_group[gi].variants;
            for (S64 vi = 0; vi < da_getsize(variants); vi++)
            {
                if (variants[vi].precision == model_precision)
                {
                    config.inf_settings.active.variant = &variants[vi];
                    break;
                }
            }
            break;
        }
    }

    return config;
}

void setup_dirs(Config *config)
{
    StringBuilder base = string_empty(config_arena, 1024);

    const char *home = os_gethome();
    Assert(home, "home dir not found");
    string_push(&base, home);

#if OS_WIN32
    win32_format_path(&base);
#endif

    if (match_end(StringCast(base), "/"))
        string_pop_by(&base, 1);

    StringBuilder app_data = string_init(config_arena, config->app_data);
    string_replace(&app_data, "~", CStrCast(base));

    StringBuilder atlas_dir = string_init(config_arena, config->atlas_dir);
    string_replace(&atlas_dir, "~", CStrCast(base));
    string_replace(&atlas_dir, "$", CStrCast(app_data));

    StringBuilder db_path = string_init(config_arena, config->db_path);
    string_replace(&db_path, "~", CStrCast(base));
    string_replace(&db_path, "$", CStrCast(app_data));

    StringBuilder model_base = string_init(config_arena, config->inf_settings.base_dir);
    string_replace(&model_base, "~", CStrCast(base));
    string_replace(&model_base, "$", CStrCast(app_data));

    os_mkdirs(StringCast(app_data));
    os_mkdirs(StringCast(atlas_dir));
    os_mkdirs(StringCast(model_base));

    config->app_data = StringCast(app_data);
    config->atlas_dir = StringCast(atlas_dir);
    config->db_path = StringCast(db_path);

    config->inf_settings.base_dir = StringCast(model_base);
}

void config_init()
{
    arena_alloc(MB(1), config_arena);

    // TODO: Put a check for existing config.yaml file
    perf_beg(config_read);
    mscbl_config = default_config();
    perf_end(config_read);
    setup_dirs(&mscbl_config);

    // if (!os_path_exist(config_path))
    // {
    //     os_mkdir()
    // }
    // else
    // {
    // FILE *cfg_file = fopen(CStrCast(config_path), "r");
    // Assert(cfg_file);
    //
    // fseek(cfg_file, 0, SEEK_END);
    // U64 size = ftell(cfg_file);
    // fseek(cfg_file, 0, SEEK_SET);
    //
    // U8 *buffer = push_array(config_arena, size, U8);
    // Assert(fread(buffer, 1, size, cfg_file) == size);
    // fclose(cfg_file);
    //
    // fy_document *fyd = fy_document_build_from_string(NULL, (const char *)buffer, size);
    // }
}
