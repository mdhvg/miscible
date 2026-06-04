#include "IconsLucide.h"
#include "base/arena.h"
#include "base/string.h"
#include "config.h"
#include "db/view.h"
#include "ui/ui_utils.h"

// .h
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "window/window.h"
#include "base/base_core.h"
#include "ui/theme.h"
#include "db/db_helpers.h"
#include "stb_image.h"

// .c, .cpp
#include "gl/gl_core.cpp"
#include "ui/ui_utils.cpp"
#include "ui/ui_debug.cpp"
#include "ui/widgets.cpp"

#if !DBG
#include "fonts/geist.cpp"
#include "fonts/lucide.cpp"
#endif

struct UIToast
{
    F32 time;
    U32 count;
    Mutex mutex;
    Result cur;
    RingBuffer(Result, queue, KB(1));
};

UIState ui_state = {0};
UIToast ui_toast = {0};
UIPreview ui_preview = {0};

void ui_viewquery_clear()
{
    string_clear(ui_state.view_query.search_query);
    arena_clear(ui_state.view_query.arena);
    ui_state.view_query.search_query = string_empty(ui_state.view_query.arena, 4096);
    ui_state.view_query.filters = NULL;
}

void ui_push_message(Result message)
{
    EnterCriticalSection(&ui_toast.mutex);

    if (!rb_isfull(ui_toast.queue))
    {
        rb_push(ui_toast.queue, message);
        ins_atomic_u32_inc_eval(&ui_toast.count);
    }

    LeaveCriticalSection(&ui_toast.mutex);
}

void ui_init()
{
    arena_alloc(MB(1), ui_state.arena);
    arena_alloc(MB(1), ui_state.page_arena);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    // io.IniFilename = NULL;
    // io.LogFilename = NULL;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;	  // Enable
    // Multi-Viewport

    io.ConfigErrorRecovery = true;
    io.ConfigErrorRecoveryEnableTooltip = true;
    io.ConfigErrorRecoveryEnableDebugLog = true;

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(win.handle, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    ImFontConfig icon_font_config;
    icon_font_config.MergeMode = true;
    icon_font_config.PixelSnapH = true;
    // icon_font_config.GlyphOffset.x = 1;
    icon_font_config.GlyphOffset.y = 3.5;
    // icon_font_config.GlyphMinAdvanceX = icon_font_size;
    static const ImWchar icons_ranges[] = {ICON_MIN_LC, ICON_MAX_LC, 0};
#if DBG
    ui_state.title_font = io.Fonts->AddFontFromFileTTF(ROOT_DIR "/fonts/Geist-VariableFont_wght.ttf", mscbl_config.settings.font_size, NULL, io.Fonts->GetGlyphRangesDefault());
    ui_state.icon_font = io.Fonts->AddFontFromFileTTF(ROOT_DIR "/fonts/Lucide.ttf", mscbl_config.settings.font_size, &icon_font_config, icons_ranges);
    ui_state.title_font = io.Fonts->AddFontFromFileTTF(ROOT_DIR "/fonts/Geist-VariableFont_wght.ttf", mscbl_config.settings.font_size * 2.0f, NULL, io.Fonts->GetGlyphRangesDefault());
#else
    ui_state.ui_font = io.Fonts->AddFontFromMemoryCompressedTTF(geist_font_compressed_data, geist_font_compressed_size, mscbl_config.settings.font_size, NULL, io.Fonts->GetGlyphRangesDefault());
    ui_state.icon_font = io.Fonts->AddFontFromMemoryCompressedTTF(lucide_font_compressed_data, lucide_font_compressed_size, mscbl_config.settings.font_size, &icon_font_config, icons_ranges);
    ui_state.ui_font = io.Fonts->AddFontFromMemoryCompressedTTF(geist_font_compressed_data, geist_font_compressed_size, mscbl_config.settings.font_size * 2.0f, NULL, io.Fonts->GetGlyphRangesDefault());

    restyle();
#endif

    // Filter list init
    arena_alloc(MB(1), ui_state.view_query.arena);
    ui_state.view_query.search_query = string_empty(ui_state.view_query.arena, 4096);

    // Toast message init
    InitializeCriticalSection(&ui_toast.mutex);
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

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("Window", NULL,
                 ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus |
                     ImGuiWindowFlags_NoNav |
                     ImGuiWindowFlags_NoDocking);

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
    if (page_data[ui_state.page].fn)
        page_data[ui_state.page].fn();
#else
    page_data[ui_state.page]();
#endif

    arena_clear(ui_state.page_arena);
    ImGui::End();

    ImGui::PopStyleVar(2);

    ImGui::ShowDemoWindow();
    ui_debug_arenas();

    if (ui_toast.time <= 0.0f)
    {
        if (ins_atomic_u32_eval(&ui_toast.count) && TryEnterCriticalSection(&ui_toast.mutex))
        {
            ins_atomic_u32_dec_eval(&ui_toast.count);
            ui_toast.cur = rb_pop(ui_toast.queue);
            ui_toast.time = 3.0f;
            LeaveCriticalSection(&ui_toast.mutex);
        }
    }

    if (ui_toast.time > 0.0f)
    {
        ImGuiViewport *viewport = ImGui::GetMainViewport();

        ImVec2 window_pos = ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - MSCBL_OUTER_PADDING, viewport->WorkPos.y + viewport->WorkSize.y - MSCBL_OUTER_PADDING);

        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(1.0f, 1.0f));

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration |
                                        ImGuiWindowFlags_AlwaysAutoResize |
                                        ImGuiWindowFlags_NoSavedSettings |
                                        ImGuiWindowFlags_NoFocusOnAppearing |
                                        ImGuiWindowFlags_NoNav |
                                        ImGuiWindowFlags_NoMove;

        ImGui::Begin("message", NULL, window_flags);
        switch (ui_toast.cur.domain)
        {
        case Domain_OS: ImGui::Text("OS Error"); break;
        case Domain_App: ImGui::Text("App Error"); break;
        case Domain_Network: ImGui::Text("Network Error"); break;
        case Domain_YAML: ImGui::Text("YAML Parsing Error"); break;
        default: break;
        }
        ImGui::Separator();
        ImGui::Text("Code (%d): %s", ui_toast.cur.code, ui_toast.cur.context);
        ImGui::ProgressBar(ui_toast.time / 3.0f, ImVec2(-FLT_MIN, MSCBL_OUTER_PADDING), "");
        ImGui::End();

        ui_toast.time -= ImGui::GetIO().DeltaTime;
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

B32 ui_needs_update()
{
    ImGuiIO &io = ImGui::GetIO();
    return ImGui::IsAnyItemHovered() || io.WantCaptureMouse || io.WantTextInput;
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
    *path = string_copy(ui_arena, sqlite3_column_text(stmt, 0));
}

void get_filename(U64 id, String *filename)
{
    sqlite3_stmt *stmt = db_prepare("SELECT filename FROM Images WHERE id = ?;");
    sqlite3_bind_int64(stmt, 1, id);
    db_run_stmt(stmt, 1, get_path, filename);
}

void ui_add_filter()
{
    UIFilter *filter_slot = NULL;

    UIFilter **walk = &ui_state.view_query.filters;
    UIFilter *last_node = NULL;

    while (*walk != NULL)
    {
        if (!(*walk)->active && !filter_slot)
        {
            filter_slot = *walk;
            *walk = filter_slot->next;

            continue;
        }

        last_node = *walk;
        walk = &((*walk)->next);
    }

    if (!filter_slot)
        filter_slot = push_struct(ui_state.view_query.arena, UIFilter);

    *filter_slot = {
        .active = 1,
        .next = NULL,
        .type = FilterType_SizeGreater,
        .exclude = 0,
        .val_bytes = {
            .value = 0,
            .unit = Byte,
        },
    };

    *walk = filter_slot;
}
