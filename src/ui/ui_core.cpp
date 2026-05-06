// ImGUI
#include "ui/ui_core.h"
#include "base/arena.h"
#include "base/array.h"
#include "base/string.h"
#include "ui/ui_utils.h"
// #include "index/index.h"
#include "sqlite3.h"
#include "ui/theme.h"
#include "IconsMaterialSymbols.h"

// .h
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "window/window.h"
#include "base/base_core.h"
#include "db/db_helpers.h"
#include "stb_image.h"

#if OS_WINDOWS
#elif OS_LINUX
#include <dlfcn.h>
#endif

// sort_params current_sort = {1, 0, OrderBy_Filename, ui_arena};
StringBuilder strbuf = {0};

// .c, .cpp
#include "gl/gl_core.cpp"
#include "ui/ui_utils.cpp"
#include "ui/ui_debug.cpp"
#include "ui/widgets.cpp"

#if !DBG
#include "fonts/inter.cpp"
#include "fonts/material.cpp"
#endif

UIState ui_state = {0};

void ui_init()
{
    arena_alloc(MB(1), ui_state.arena);
    arena_alloc(KB(10), ui_state.page_arena);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.IniFilename = NULL;
    io.LogFilename = NULL;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;	  // Enable
    // Multi-Viewport

    io.ConfigErrorRecoveryEnableTooltip = true;
    io.ConfigErrorRecovery = true;
    io.ConfigErrorRecoveryEnableDebugLog = true;

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(win.handle, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    static const ImWchar icons_ranges[] = {ICON_MIN_MS, ICON_MAX_MS, 0};
    float icon_font_size = 16 * 1.5f;
    ImFontConfig icon_font_config;
    icon_font_config.MergeMode = true;
    icon_font_config.PixelSnapH = true;
    icon_font_config.GlyphOffset.y = 5;
    icon_font_config.GlyphOffset.x = 2;
    // icon_font_config.GlyphMinAdvanceX = icon_font_size;
#if DBG
    ui_state.title_font = io.Fonts->AddFontFromFileTTF(ROOT_DIR "/fonts/Geist-VariableFont_wght.ttf", 16, NULL, io.Fonts->GetGlyphRangesDefault());
    ui_state.icon_font = io.Fonts->AddFontFromFileTTF(ROOT_DIR "/fonts/MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf", icon_font_size, &icon_font_config, icons_ranges);
    ui_state.title_font = io.Fonts->AddFontFromFileTTF(ROOT_DIR "/fonts/Geist-VariableFont_wght.ttf", 36, NULL, io.Fonts->GetGlyphRangesDefault());
#else
    ui_state.ui_font = io.Fonts->AddFontFromMemoryCompressedTTF(inter_font_compressed_data, inter_font_compressed_size, 16, NULL, io.Fonts->GetGlyphRangesDefault());
    ui_state.icon_font = io.Fonts->AddFontFromMemoryCompressedTTF(material_font_compressed_data, material_font_compressed_size, icon_font_size, &icon_font_config, icons_ranges);
    ui_state.ui_font = io.Fonts->AddFontFromMemoryCompressedTTF(inter_font_compressed_data, inter_font_compressed_size, 36, NULL, io.Fonts->GetGlyphRangesDefault());
#endif

    // arena_alloc(MB(1), ui_arena);
    // ui_state.search_buffer = string_empty(ui_arena, 512);
    // strbuf                 = string_empty(ui_arena, 512);
    // current_sort           = {1, 0, OrderBy_Filename, ui_arena};
    // async_job(os_info.pool, index_fill, NULL);

#if !DBG
    restyle();
#endif

    // TODO: Fill data? (This will load all dir names and make ImGui::TreeNode(...)) with it

    ui_filterlist_init();
}

void ui_filterlist_init()
{
    arena_alloc(MB(1), ui_state.view_query.arena);
    da_push(ui_state.view_query.arena, ui_state.view_query.filters, {0});
    ui_state.view_query.search_query = string_empty(ui_state.view_query.arena, 4096);
}

void ui_close()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ui_update()
{
    // refresh_results();
}

void ui_render()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(viewport->Size);

    ImGui::Begin("Window", NULL,
                 ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus |
                     ImGuiWindowFlags_NoNav);

    // ImGui::BeginMenuBar();
    // if (ImGui::BeginMenu("File"))
    // {
    //     if (ImGui::MenuItem("Exit"))
    //         glfwSetWindowShouldClose(win.handle, GLFW_TRUE);
    //     ImGui::EndMenu();
    // }
    // ImGui::EndMenuBar();

#if DBG
    if (ImGui::IsKeyPressed(ImGuiKey_F5))
    {
        ui_reload();
        if (restyle)
            restyle();
    }

    // Render page
    {
        if (page_data[ui_state.page].fn)
            page_data[ui_state.page].fn();
#else
    page_data[ui_state.page]();
#endif

        arena_clear(ui_state.page_arena);
    }
    ImGui::End();

    ImGui::ShowDemoWindow();
    ui_debug_arenas();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

U64 put_dir(StringBuilder dir)
{
    sqlite3_stmt *stmt = db_prepare("INSERT INTO Dirs(path) VALUES(?) RETURNING id;");
#if OS_WINDOWS
    win32_format_path(&dir);
#endif
    sqlite3_bind_text(stmt, 1, CStrCast(dir), dir.size, SQLITE_STATIC);
    U64 id = 0;
    db_run_stmt(stmt, 1, get_count, &id);
    return id;
}

DBStmtCbk(get_path)
{
    String *path = (String *)data;
    *path = string_cpy(ui_arena, sqlite3_column_text(stmt, 0));
}

void get_filename(U64 id, String *filename)
{
    sqlite3_stmt *stmt = db_prepare("SELECT filename FROM Images WHERE id = ?;");
    sqlite3_bind_int64(stmt, 1, id);
    db_run_stmt(stmt, 1, get_path, filename);
}

DBStmtCbk(fetch_info)
{
    Image *img = (Image *)data;
    img->path = string_cpy(ui_arena, sqlite3_column_text(stmt, 0));
    img->filename = string_cpy(ui_arena, sqlite3_column_text(stmt, 1));
    img->size = sqlite3_column_int64(stmt, 2);
    img->mtime = sqlite3_column_int64(stmt, 3);
    img->ctime = sqlite3_column_int64(stmt, 4);
    img->width = sqlite3_column_int(stmt, 5);
    img->height = sqlite3_column_int(stmt, 6);
    img->channels = sqlite3_column_int(stmt, 7);
}

void get_info(U64 id, Image *img)
{
    sqlite3_stmt *stmt = db_prepare("SELECT path, filename, size, mtime, ctime, width, height, channels FROM Images WHERE id = ?;");
    sqlite3_bind_int64(stmt, 1, id);
    db_run_stmt(stmt, 1, fetch_info, img);
}
