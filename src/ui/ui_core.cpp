// ImGUI
#include <dlfcn.h>
#include "base/log.h"
#include "ui/ui_core.h"
#include "ui/theme.h"
#include "IconsMaterialSymbols.h"
#include "base/arena.h"
#include "base/array.h"
#include "base/tree.h"

// .h
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
// #include "atlas/atlas_render.h"
#include "window/window.h"
#include "base/base_core.h"
#include "base/threadpool.h"
#include "db/db_helpers.h"
#include "os/os_inc.h"
#include "gl/gl_core.h"
#include "stb_image.h"

// .c, .cpp
#include "gl/gl_core.cpp"

global_v void *pages  = NULL;
global_v UIfn restyle = NULL;

UIState ui_state     = {0};
StringBuilder strbuf = {0};

void ui_reload()
{
    if (pages)
        dlclose(pages);
    pages   = dlopen("build/pages.so", RTLD_NOW);
    restyle = (UIfn)dlsym(pages, "restyle");

    for (S32 i = 0; i < UIPage_COUNT; i++)
    {
        page_data[i].fn = (UIfn)dlsym(pages, page_data[i].fn_name);
    }
}

void ui_init()
{
    // arena_db_alloc(&ui_persist.ui_arena, KB(100));
    // ui_persist.search_buffer = push_array0(mscbl.persistent_arena, 2048, U8);

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

    io.ConfigErrorRecoveryEnableTooltip  = true;
    io.ConfigErrorRecovery               = true;
    io.ConfigErrorRecoveryEnableDebugLog = true;

    // Setup Dear ImGui style
    // ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(win.handle, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    ui_state.ui_font = io.Fonts->AddFontFromFileTTF(ROOT_DIR "/fonts/Geist-VariableFont_wght.ttf", 16, NULL, io.Fonts->GetGlyphRangesDefault());

    static const ImWchar icons_ranges[] = {ICON_MIN_MS, ICON_MAX_MS, 0};
    float icon_font_size                = 16 * 1.5f;
    ImFontConfig icon_font_config;
    icon_font_config.MergeMode     = true;
    icon_font_config.PixelSnapH    = true;
    icon_font_config.GlyphOffset.y = 5;
    icon_font_config.GlyphOffset.x = 2;
    // icon_font_config.GlyphMinAdvanceX = icon_font_size;
    ui_state.icon_font  = io.Fonts->AddFontFromFileTTF(ROOT_DIR "/fonts/MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf", icon_font_size, &icon_font_config, icons_ranges);
    ui_state.title_font = io.Fonts->AddFontFromFileTTF(ROOT_DIR "/fonts/Geist-VariableFont_wght.ttf", 36, NULL, io.Fonts->GetGlyphRangesDefault());
    // PERF(SET_ORDER, set_order(state.sorting));

    if (!strbuf.capacity)
        strbuf = string_empty(mscbl.persistent_arena, 512);

    ui_image_tree();
}

void ui_close()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

DB_STMT_CBK(push_images)
{
    Image img     = {0};
    img.id        = sqlite3_column_int64(stmt, 0);
    img.atlas_id  = sqlite3_column_int64(stmt, 1);
    img.atlas_idx = sqlite3_column_int(stmt, 2);
    Image_Node *n = tree_node(mscbl.persistent_arena, img, Image);
    tree_push(&ui_state.images, n, Image_cmp, Image);
}

void ui_image_tree()
{
    sqlite3_stmt *stmt = db_prepare("SELECT id, atlas_id, atlas_idx FROM Images;");
    db_run_stmt(stmt, 1, push_images);
}

void ui_update()
{
    // arena_db_switch(&ui_persist.ui_arena);

    // if (state.active_image != prev.last_image)
    // {
    // 	// threadpool_enqueue({load_texture_2d, });
    // 	prev.last_image = state.active_image;
    // }

    // state.fps = 1 / delta;
    // if (state.sorting != prev.sorting)
    // {
    // 	PERF(SET_ORDER, set_order(state.sorting));
    // 	prev.sorting = state.sorting;
    // }

    // if (Application::get_instance().img_man.images.size() != image_count) {
    //   PERF(SET_ORDER, set_order(state.sorting));
    //   image_count = Application::get_instance().img_man.images.size();
    // }
}

// internal void set_order(SortMode selection)
// {
// 	const char *order_str;
// 	switch (selection)
// 	{
// 	case SortMode::SORT_FILENAME:
// 		order_str = "filename";
// 	default:
// 		order_str = "filename";
// 	}

// 	std::vector<unsigned int> temp;
// 	String statement = string_format(mscbl.ui_arena->arena, "SELECT id FROM Images ORDER BY %s;", order_str);
// 	db_run(
// 		(const char *)statement.value,
// 		[](void *data, int, char **argv, char **) {
// 			auto temp = (std::vector<unsigned int> *)data;
// 			temp->push_back(strtoul(argv[0], NULL, 0));
// 			return 0;
// 		},
// 		&temp);
// 	order.swap(temp);
// }

// inline const char *SortToString(SortMode s)
// {
// 	switch (s)
// 	{
// 	case SortMode::SORT_FILENAME:
// 		return "Filename";
// 	default:
// 		return "Unknown";
// 	}
// }

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

    if (ImGui::IsKeyPressed(ImGuiKey_F5))
    {
        ui_reload();
        if (restyle)
            restyle();
    }
    if (page_data[ui_state.page].fn)
        page_data[ui_state.page].fn();

    ImGui::End();

    ImGui::ShowDemoWindow();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

DB_CALLBACK(push_image_ptr)
{
    U64 *count                         = (U64 *)data;
    U64 id                             = strtoull(argv[0], NULL, 0);
    Image_Node *n                      = tree_find(&ui_state.images, (Image *)&id, Image_cmp, Image);
    ui_state.display_order[(*count)++] = &n->v;
    return 0;
}

THREAD_FUNC(ui_reload_order)
{
    // TODO: Add sorting enum code
    db_run("SELECT COUNT(id) FROM Images;", get_count, &ui_state.image_count);

    ui_state.display_order = push_array(scan_arena, ui_state.image_count, Image *);
    U64 count              = 0;
    db_run("SELECT id FROM Images ORDER BY id ASC;", push_image_ptr, &count);
    mscbl_log("Order loaded");
}
