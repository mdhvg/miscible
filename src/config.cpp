// #include "libfyaml.h"

#include "config.h"
#include "base/log.h"
#include "base/string.h"
#include "os/os_inc.h"

Config mscbl_config = {0};
Arena *config_arena = NULL;

/*
 * Reserved symbols
 * '~' is for user home directory (C:/Users/user on Win32 or /home/user on Unix)
 * '$' is for app data directory
 *
 * For eg. if app_data is ~/Miscible, "$/atlas" resolves to ~/Miscible/atlas
 */

Config default_config()
{
    Config config = {
        .app_data = sv("~/Miscible"),
        .atlas_dir = sv("$/atlas"),
        .db_path = sv("$/miscible.sqlite"),
        .settings = {
            .scan_depth = 15,
            .font_size = 16.0f},
        .view_settings = {
            .sort_basis = SortType_DateModified,
            .descending = 0,
        }};
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

    StringBuilder app_data = string_empty(config_arena, 1024);
    string_push(&app_data, config->app_data);
    string_replace(&app_data, "~", CStrCast(base));

    StringBuilder atlas_dir = string_empty(config_arena, 1024);
    string_push(&atlas_dir, config->atlas_dir);
    string_replace(&atlas_dir, "~", CStrCast(base));
    string_replace(&atlas_dir, "$", CStrCast(app_data));

    StringBuilder db_path = string_empty(config_arena, 1024);
    string_push(&db_path, config->db_path);
    string_replace(&db_path, "~", CStrCast(base));
    string_replace(&db_path, "$", CStrCast(app_data));

    os_mkdir(StringCast(app_data));
    os_mkdir(StringCast(atlas_dir));

    config->app_data = StringCast(app_data);
    config->atlas_dir = StringCast(atlas_dir);
    config->db_path = StringCast(db_path);
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
