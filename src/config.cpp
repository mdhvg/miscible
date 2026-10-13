// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "sha2.h"
#include "libfyaml.h"
#include "libfyaml/libfyaml-core.h"
#include "libfyaml/libfyaml-util.h"

#include "yaml.h"
#include "config.h"
#include "db/view.h"
#include "base/log.h"
#include "base/array.h"
#include "base/string.h"
#include "app/miscible.h"
#include "inference/manifest.h"

Config mscbl_config = {0};
Str8 config_path = {0};
B32 config_dirty = 0;

/*
 * Reserved symbols
 * '~' is for user home directory (C:/Users/user on Win32 or /home/user on Unix)
 * '$' is for app data directory
 *
 * For eg. if app_data is ~/Miscible, "$/atlas" resolves to ~/Miscible/atlas
 */

Config config_parse(Arena *arena, Str8 config_content)
{
    fy_document *config_doc = fy_document_build_from_string(NULL, CStrCast(config_content), config_content.size);
    Assert(config_doc, "fyd is NULL");
    fy_node *config_root = fy_document_root(config_doc);

    Config config = {
        .app_data = yaml_scan_string(arena, config_root, "/AppData"),
        .atlas_dir = yaml_scan_string(arena, config_root, "/AtlasDir"),
        .db_path = yaml_scan_string(arena, config_root, "/DBPath"),

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
#if OS_WIN32
            .dml_device = (U32)yaml_scan_int(config_root, "/Inference/Hardware"),
#elif OS_LINUX
#endif
            .base_dir = yaml_scan_string(arena, config_root, "/Inference/BaseDir"), // To stop formatter from collapsing these
        },
    };

    Str8 backend = yaml_scan_string(arena, fy_node_by_path(config_root, "/Inference/Active", FY_NT, FYNWF_FOLLOW), "/Backend");
    Str8 model_name = yaml_scan_string(arena, fy_node_by_path(config_root, "/Inference/Active", FY_NT, FYNWF_FOLLOW), "/Name");
    U64 model_precision = yaml_scan_int(fy_node_by_path(config_root, "/Inference/Active", FY_NT, FYNWF_FOLLOW), "/Precision");

    ModelGroup *backend_group = NULL;
    U64 backend_group_size = 0;
    if (str8_cmp(backend, sv8("ONNX")))
    {
        config.inf_settings.active.backend = Backend_ONNX;
        backend_group = onnx_groups;
        backend_group_size = onnx_groups_size;
    }
    else
    {
        config.inf_settings.active.backend = Backend_GGML;
        backend_group = ggml_groups;
        backend_group_size = ggml_groups_size;
    }

    for (S64 gi = 0; gi < backend_group_size; gi++)
    {
        if (str8_cmp(backend_group[gi].name, model_name))
        {
            config.inf_settings.active.group = &backend_group[gi];
            for (S64 vi = 0; vi < backend_group[gi].variants_size; vi++)
            {
                if (backend_group[gi].variants[vi].precision == model_precision)
                {
                    config.inf_settings.active.variant = &backend_group[gi].variants[vi];
                    break;
                }
            }
            break;
        }
    }

    fy_document_destroy(config_doc);
    return config;
}

void setup_dirs(Arena *arena, Config *config)
{
    StringBuilder base = str8_empty(arena, 1024);

    const char *home = os_gethome();
    Assert(home, "home dir not found");
    str8_push(&base, home);

#if OS_WIN32
    win32_format_path(&base);
#endif

    if (str8_match_end(Str8Cast(base), sv8_lit("/")))
        str8_pop_by(&base, 1);

    StringBuilder app_data = str8_init(arena, config->app_data);
    str8_replace(&app_data, sv8_lit("~"), Str8Cast(base));

    StringBuilder atlas_dir = str8_init(arena, config->atlas_dir);
    str8_replace(&atlas_dir,sv8_lit( "~"), Str8Cast(base));
    str8_replace(&atlas_dir, sv8_lit("$"), Str8Cast(app_data));

    StringBuilder db_path = str8_init(arena, config->db_path);
    str8_replace(&db_path, sv8_lit("~"), Str8Cast(base));
    str8_replace(&db_path, sv8_lit("$"), Str8Cast(app_data));

    StringBuilder model_base = str8_init(arena, config->inf_settings.base_dir);
    str8_replace(&model_base, sv8_lit("~"), Str8Cast(base));
    str8_replace(&model_base, sv8_lit("$"), Str8Cast(app_data));

    os_mkdirs(Str8Cast(app_data));
    os_mkdirs(Str8Cast(atlas_dir));
    os_mkdirs(Str8Cast(model_base));

    config->app_data = Str8Cast(app_data);
    config->atlas_dir = Str8Cast(atlas_dir);
    config->db_path = Str8Cast(db_path);

    config->inf_settings.base_dir = Str8Cast(model_base);
}

void config_init(Arena *arena)
{
    Str8 root = os_env_var(APP_HOME_ENV, arena);
    Str8 append = sv8(APP_APPEND);

    StringBuilder base = str8_init(arena, root);
    str8_path_join(&base, append);
    str8_path_join(&base, sv8_lit("config.yml"));

    config_path = Str8Cast(base);

    Result res = ResultSuccess();
    if (os_path_exists(Str8Cast(base), &res))
    {
        FileHandle config_handle = os_file_open(Str8Cast(base), FileAccess_Read, FileMode_OpenAlways, &res);
        U64 config_file_size = os_file_size(config_handle, &res);

        U8 *content_buffer = push_array(arena, config_file_size, U8);
        os_file_read(config_handle, config_file_size, content_buffer, &res);
        os_file_close(config_handle, &res);
        Str8 config_content = sv8(content_buffer, config_file_size);

        mscbl_config = config_parse(arena, Str8Cast(config_content));
    }
    else
    {
#include "config.yml"
        Str8 config_content = sv8_lit(__mscbl_cfg_text);
        mscbl_config = config_parse(arena, config_content);
        config_dirty = 1;
    }

    setup_dirs(arena, &mscbl_config);
}

void config_deinit(Arena *arena)
{
    if (!config_dirty) return;

    // Serialize config
    struct fy_document *config_doc = fy_document_create(NULL);
    struct fy_node *config_root = fy_node_create_mapping(config_doc);
    fy_document_set_root(config_doc, config_root);

    fy_node_mapping_append(config_root,
                           fy_node_create_scalar(config_doc, "AppData", FY_NT),
                           fy_node_create_scalar(config_doc, CStrCast(mscbl_config.app_data), FY_NT));

    fy_node_mapping_append(config_root,
                           fy_node_create_scalar(config_doc, "AtlasDir", FY_NT),
                           fy_node_create_scalar(config_doc, CStrCast(mscbl_config.atlas_dir), FY_NT));

    fy_node_mapping_append(config_root,
                           fy_node_create_scalar(config_doc, "DBPath", FY_NT),
                           fy_node_create_scalar(config_doc, CStrCast(mscbl_config.db_path), FY_NT));

    {
        fy_node *settings = fy_node_create_mapping(config_doc);
        fy_node_mapping_append(settings,
                               fy_node_create_scalar(config_doc, "LogAge", FY_NT),
                               fy_node_create_scalarf(config_doc, "%d", mscbl_config.settings.log_age_days));

        fy_node_mapping_append(settings,
                               fy_node_create_scalar(config_doc, "ScanDepth", FY_NT),
                               fy_node_create_scalarf(config_doc, "%d", mscbl_config.settings.scan_depth));

        fy_node_mapping_append(settings,
                               fy_node_create_scalar(config_doc, "FontSize", FY_NT),
                               fy_node_create_scalarf(config_doc, "%.1f", mscbl_config.settings.font_size));

        fy_node_mapping_append(config_root,
                               fy_node_create_scalar(config_doc, "Settings", FY_NT),
                               settings);
    }

    {
        fy_node *view_settings = fy_node_create_mapping(config_doc);
        fy_node_mapping_append(view_settings,
                               fy_node_create_scalar(config_doc, "SortBasis", FY_NT),
                               fy_node_create_scalarf(config_doc, "%d", mscbl_config.view_settings.sort_basis));

        fy_node_mapping_append(view_settings,
                               fy_node_create_scalar(config_doc, "Descending", FY_NT),
                               fy_node_create_scalarf(config_doc, "%d", mscbl_config.view_settings.descending));

        fy_node_mapping_append(config_root,
                               fy_node_create_scalar(config_doc, "ViewSettings", FY_NT),
                               view_settings);
    }

    {
        fy_node *inf_settings = fy_node_create_mapping(config_doc);
        fy_node_mapping_append(inf_settings,
                               fy_node_create_scalar(config_doc, "BaseDir", FY_NT),
                               fy_node_create_scalar(config_doc, CStrCast(mscbl_config.inf_settings.base_dir), FY_NT));
        fy_node_mapping_append(inf_settings,
                               fy_node_create_scalar(config_doc, "Hardware", FY_NT),
#if OS_WIN32
                               fy_node_create_scalarf(config_doc, "%u", mscbl_config.inf_settings.dml_device)
#elif OS_LINUX
#endif
        );
        {
            fy_node *active = fy_node_create_mapping(config_doc);

            fy_node_mapping_append(active,
                                   fy_node_create_scalar(config_doc, "Backend", FY_NT),
                                   fy_node_create_scalar(config_doc, mscbl_config.inf_settings.active.backend == Backend_GGML ? "GGML" : "ONNX", FY_NT));

            fy_node_mapping_append(active,
                                   fy_node_create_scalar(config_doc, "Precision", FY_NT),
                                   fy_node_create_scalarf(config_doc, "%d", mscbl_config.inf_settings.active.variant->precision));

            fy_node_mapping_append(active,
                                   fy_node_create_scalar(config_doc, "Name", FY_NT),
                                   fy_node_create_scalar(config_doc, CStrCast(mscbl_config.inf_settings.active.group->name), FY_NT));

            fy_node_mapping_append(inf_settings,
                                   fy_node_create_scalar(config_doc, "Active", FY_NT),
                                   active);
        }

        fy_node_mapping_append(config_root,
                               fy_node_create_scalar(config_doc, "Inference", FY_NT),
                               inf_settings);
    }

    fy_emit_document_to_file(config_doc, FYECF_DEFAULT, CStrCast(config_path));
    fy_document_destroy(config_doc);
}
