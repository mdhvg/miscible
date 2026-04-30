// #include "libfyaml.h"

#include "config.h"
#include "base/log.h"
#include "base/arena.h"
#include "os/os_inc.h"

Config mscbl_config = {0};
Arena *config_arena = NULL;

void default_config()
{
    Settings def   = {0};
    def.font_size  = 16.0f;
    def.scan_depth = 15;

    mscbl_config.settings = def;
}

void setup_dirs()
{
    StringBuilder base = string_empty(config_arena, 1024);

    const char *home = os_gethome();
    Assert(home, "Home dir not found");
    string_push(&base, home);

#if OS_WINDOWS
    win32_format_path(&base);
#endif

    if (!match_end(StringCast(base), "/"))
        string_push(&base, "/");

    string_push(&base, Stringify(APP_NAME));
    os_mkdir(StringCast(base));
    mscbl_config.home_path = string_cpy(config_arena, StringCast(base));

    U64 size = base.size;
    string_push(&base, "/" ATLAS_DIR);
    os_mkdir(StringCast(base));
    mscbl_config.atlas_dir = string_cpy(config_arena, StringCast(base));

    string_pop_to(&base, size);
    string_push(&base, "/" DB_FILE);
    mscbl_config.db_path = string_cpy(config_arena, StringCast(base));
}

void config_init()
{
    arena_alloc(MB(1), config_arena);
    setup_dirs();

    // TODO: Put a check for existing config.yaml file
    default_config();

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
