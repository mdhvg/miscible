#include "libfyaml.h"
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

ModelConfig config_load_model(fy_node *model_node, U8 *str_buf)
{
    ModelConfig model_cfg = {
        .filename = yaml_scan_string(config_arena, model_node, str_buf, "/Filename")};

    yaml_scan_hash(model_node, str_buf, model_cfg.model_hash, SHA512_DIGEST_SIZE, "/ModelHash");
    yaml_scan_hash(model_node, str_buf, model_cfg.manifest_hash, SHA512_DIGEST_SIZE, "/ManifestHash");

    fy_node *model_url_node = fy_node_by_path(model_node, "/ModelURL", FY_NT, FYNWF_FOLLOW);
    fy_node *manifest_url_node = fy_node_by_path(model_node, "/ManifestURL", FY_NT, FYNWF_FOLLOW);
    Assert(!fy_node_sequence_is_empty(model_url_node), "model url array empty");
    Assert(!fy_node_sequence_is_empty(manifest_url_node), "manifest url array empty");

    S64 model_url_count = fy_node_sequence_item_count(model_url_node);
    S64 manifest_url_count = fy_node_sequence_item_count(manifest_url_node);
    da_setcap(config_arena, model_cfg.model_url, model_url_count);
    da_setcap(config_arena, model_cfg.manifest_url, manifest_url_count);

    {
        fy_node *url_node = NULL;
        void *pre = NULL;
        while ((url_node = fy_node_sequence_iterate(model_url_node, &pre)))
        {
            String url = yaml_scan_string(config_arena, url_node, str_buf, "/");
            da_push(config_arena, model_cfg.model_url, url);
        }
    }

    {
        fy_node *url_node = NULL;
        void *pre = NULL;
        while ((url_node = fy_node_sequence_iterate(manifest_url_node, &pre)))
        {
            String url = yaml_scan_string(config_arena, url_node, str_buf, "/");
            da_push(config_arena, model_cfg.manifest_url, url);
        }
    }

    return model_cfg;
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

    U8 *str_buf = push_array(config_arena, KB(4), U8);

    fy_document *config_doc = fy_document_build_from_string(NULL, CStrCast(config_text), config_text.size);
    Assert(config_doc, "fyd is NULL");
    fy_node *config_root = fy_document_root(config_doc);

    Config config = {
        .app_data = yaml_scan_string(config_arena, config_root, str_buf, "/AppData"),
        .atlas_dir = yaml_scan_string(config_arena, config_root, str_buf, "/AtlasDir"),
        .db_path = yaml_scan_string(config_arena, config_root, str_buf, "/DBPath"),

        .settings = {.scan_depth = yaml_scan_int(config_root, "/Settings/ScanDepth"),
                     .font_size = yaml_scan_float(config_root, "/Settings/FontSize"),
                     .log_age_days = yaml_scan_int(config_root, "/Settings/LogAge")},

        .view_settings = {
            .sort_basis = (SortType)yaml_scan_int(config_root, "/ViewSettings/SortBasis"),
            .descending = ((yaml_scan_int(config_root, "/ViewSettings/Descending") == 0) ? 0 : 1)},

        .model_group = {
            .base_dir = yaml_scan_string(config_arena, config_root, str_buf, "/Models/BaseDir"),
            .clip_model = config_load_model(fy_node_by_path(config_root, "/Models/CLIPModel", FY_NT, FYNWF_FOLLOW), str_buf),
        },
    };

    return config;
}

void setup_dirs(Config *config)
{
    StringBuilder base = string_empty(config_arena, 1024);

    const char *home = os_gethome();
    Assert(home, "home dir not found");
    string_push(&base, home);

#if OS_WINDOWS
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

    StringBuilder model_base = string_init(config_arena, config->model_group.base_dir);
    string_replace(&model_base, "~", CStrCast(base));
    string_replace(&model_base, "$", CStrCast(app_data));

    StringBuilder clip_model_path = string_init(config_arena, StringCast(model_base));
    path_join(&clip_model_path, config->model_group.clip_model.filename);

    os_mkdirs(StringCast(app_data));
    os_mkdirs(StringCast(atlas_dir));
    os_mkdirs(StringCast(model_base));

    config->app_data = StringCast(app_data);
    config->atlas_dir = StringCast(atlas_dir);
    config->db_path = StringCast(db_path);
    config->model_group.base_dir = StringCast(model_base);
    config->model_group.clip_model.path = StringCast(clip_model_path);
}

void config_init()
{
    arena_alloc(MB(1), config_arena);

    // TODO: Put a check for existing config.yaml file
    mscbl_config = default_config();
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
