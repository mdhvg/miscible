// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "IconsLucide.h"
#include "app/miscible.h"
#include "base/arena.h"
#include "base/ringbuf.h"
#include "base/string.h"
#include "base/threadpool.h"
#include "config.h"
#include "db/view.h"
#include "os/os_inc.h"
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

#if !DBG
#include "_dynamic/instrument.c"
#include "_dynamic/lucide.c"
#include "_dynamic/geist.c"
#include "_dynamic/icon.c"
#endif

struct UIToast
{
    F32 time;
    U32 count;
    Mutex mutex;
    Result cur;
    RingBuffer_t(Result) queue;
};

UIState ui_state = {0};
UIToast ui_toast = {.time = 0};
UIPreview ui_preview = {0};
UIResult ui_result = {};

void ui_viewquery_clear()
{
    string_clear(ui_state.view_query.search_query);
    arena_clear(ui_state.view_query.arena);
    ui_state.view_query.search_query = string_empty(ui_state.view_query.arena, 4096);
    ui_state.view_query.filters = NULL;
}

void ui_push_message(Result message)
{
    os_mutex_lock(&ui_toast.mutex);

    if (!rb_isfull(ui_toast.queue))
    {
        rb_push(ui_toast.queue, message);
        ins_atomic_u32_inc_eval(&ui_toast.count);
    }

    os_mutex_unlock(&ui_toast.mutex);
}

void ui_init()
{
    arena_alloc(MB(1), ui_state.arena);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
#if !DBG
    io.IniFilename = NULL;
    io.LogFilename = NULL;
#endif
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
    // NOTE: This is because of the comment at imgui_draw.cpp(3753)
    icon_font_config.FontDataOwnedByAtlas = false;
    static const ImWchar icons_ranges[] = {ICON_MIN_LC, ICON_MAX_LC, 0};
#if DBG
    ui_state.ui_font = io.Fonts->AddFontFromFileTTF(ROOT_DIR "/fonts/Geist.ttf", mscbl_config.settings.font_size, NULL, io.Fonts->GetGlyphRangesDefault());
    ui_state.icon_font = io.Fonts->AddFontFromFileTTF(ROOT_DIR "/fonts/Lucide.ttf", mscbl_config.settings.font_size, &icon_font_config, icons_ranges);
    ui_state.title_font = io.Fonts->AddFontFromFileTTF(ROOT_DIR "/fonts/Geist.ttf", mscbl_config.settings.font_size * 1.125f, NULL, io.Fonts->GetGlyphRangesDefault());
    ui_state.display_font = io.Fonts->AddFontFromFileTTF(ROOT_DIR "/fonts/InstrumentSans.ttf", mscbl_config.settings.font_size * 1.25f, NULL, io.Fonts->GetGlyphRangesDefault());

    AsyncTask icon_load_task = {
        .func = gl_tex_path,
        .args = {
            {.kind = TPData_Any, .val_any = &ui_state.icon_texture},
            {.kind = TPData_String, .val_str = sv(ROOT_DIR "/data/icon.png")},
        }};
#else
    // NOTE: This is because of the comment at imgui_draw.cpp(3753)
    ImFontConfig __font_config;
    __font_config.FontDataOwnedByAtlas = false;
    ui_state.ui_font = io.Fonts->AddFontFromMemoryTTF((void *)geist_font_data, geist_font_size, mscbl_config.settings.font_size, &__font_config, io.Fonts->GetGlyphRangesDefault());
    ui_state.icon_font = io.Fonts->AddFontFromMemoryTTF((void *)lucide_font_data, lucide_font_size, mscbl_config.settings.font_size, &icon_font_config, icons_ranges);
    ui_state.title_font = io.Fonts->AddFontFromMemoryTTF((void *)geist_font_data, geist_font_size, mscbl_config.settings.font_size * 1.125f, &__font_config, io.Fonts->GetGlyphRangesDefault());
    ui_state.display_font = io.Fonts->AddFontFromMemoryTTF((void *)instrumentsans_font_data, instrumentsans_font_size, mscbl_config.settings.font_size * 1.25f, &__font_config, io.Fonts->GetGlyphRangesDefault());

    AsyncTask icon_load_task = {
        .func = gl_tex_mem,
        .args = {
            {.kind = TPData_Any, .val_any = &ui_state.icon_texture},
            {.kind = TPData_Any, .val_any = (void *)icon_data},
            {.kind = TPData_U64, .val_u64 = icon_size},
        }};

    restyle();
#endif

    threadpool_enqueue(TaskPriority_High, icon_load_task);

    // Filter list init
    arena_alloc(MB(1), ui_state.view_query.arena);
    arena_alloc(MB(1), ui_result.back_map.arena);
    arena_alloc(MB(1), ui_result.main_map.arena);
    arena_alloc(MB(10), ui_result.back_list.arena);
    arena_alloc(MB(10), ui_result.main_list.arena);
    ui_state.view_query.search_query = string_empty(ui_state.view_query.arena, 4096);

    // Toast message init
    os_mutex_init(&ui_toast.mutex);
    rb_init(ui_state.arena, ui_toast.queue, KB(4));
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

void ui_window_controls()
{
    F32 padding_scale = 1.5f;
    ImVec2 window_dim = ImGui::GetMainViewport()->Size;
    ImGuiStyle &style = ImGui::GetStyle();
    ImVec2 trafficlight_size = ImGui::CalcTextSize(ICON_LC_MINUS ICON_LC_MAXIMIZE ICON_LC_X);
    trafficlight_size.x += 6.0f * style.FramePadding.x * padding_scale;
    trafficlight_size.y += 2.0f * style.FramePadding.y * padding_scale;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x * padding_scale, style.FramePadding.y * padding_scale));
    ImGui::SetNextWindowSize(trafficlight_size);
    ImGui::SetNextWindowPos(ImVec2(window_dim.x - trafficlight_size.x, 0.0f));
    DeferLoop(ImGui::Begin("Traffic light", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove), ImGui::End())
    {
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button(ICON_LC_MINUS))
        {
            window_iconify();
        }
        ImGui::SameLine(0, 0);
        if (ImGui::Button(win.maximized ? ICON_LC_MINIMIZE : ICON_LC_MAXIMIZE))
        {
            win.maximized ? window_minimize() : window_maximize();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.15f, 0.15f, 1.0f));
        if (ImGui::Button(ICON_LC_X))
        {
            window_close();
        }
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar(4);
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

    ImGui::End();

    ImGui::PopStyleVar(2);

#if DBG
    ImGui::ShowDemoWindow();
    ImGui::Begin("Arena Debug");

    U64 total = 0, total_used = 0;
    for (Arena *cur = arena_head; cur; cur = cur->next)
    {
        ImGui::Text("Arena: %s", cur->name);

        ByteSize used = size_to_bytesize(cur->used);
        ByteSize cap = size_to_bytesize(cur->capacity);
        ImGui::Text("Used %.3f%s of %.3f%s", used.value, CStrCast(byte_string(used.unit)), cap.value, CStrCast(byte_string(cap.unit)));
        ImGui::ProgressBar((F64)cur->used / (F64)cur->capacity);
        total += cur->capacity;
        total_used += cur->used;
    }

    ByteSize used = size_to_bytesize(total_used);
    ByteSize cap = size_to_bytesize(total);
    ImGui::Text("Total used: %.3f%s of %.3f%s", used.value, CStrCast(byte_string(used.unit)), cap.value, CStrCast(byte_string(cap.unit)));
    ImGui::ProgressBar((F64)total_used / (F64)total);

    ImGui::End();
#endif

    if (ui_toast.time <= 0.0f)
    {
        if (ins_atomic_u32_eval(&ui_toast.count) && os_mutex_trylock(&ui_toast.mutex))
        {
            ins_atomic_u32_dec_eval(&ui_toast.count);
            rb_top(ui_toast.queue, &ui_toast.cur);
            rb_pop(ui_toast.queue);
            ui_toast.time = 3.0f;
            os_mutex_unlock(&ui_toast.mutex);
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
        case Domain_Inference_ONNX: ImGui::Text("ONNX Runtime Error"); break;
        default: break;
        }
        ImGui::Separator();
        ImGui::Text("Code (%d): %s", ui_toast.cur.code, ui_toast.cur.context);
        ImGui::ProgressBar(ui_toast.time / 3.0f, ImVec2(-FLT_MIN, MSCBL_OUTER_PADDING), "");
        ImGui::End();

        ui_toast.time -= ImGui::GetIO().DeltaTime;
    }

    ui_window_controls();

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
#if OS_WIN32
    win32_format_path(&dir);
#endif
    sqlite3_bind_text(stmt, 1, CStrCast(dir), dir.size, SQLITE_STATIC);
    U64 id = 0;
    db_run_stmt(stmt, 1, get_count, &id);
    return id;
}

void ui_add_filter()
{
    UIFilter *filter_slot = NULL;
    UIFilter **walk = &ui_state.view_query.filters;

    while (*walk != NULL)
    {
        if (!(*walk)->active && !filter_slot)
        {
            filter_slot = *walk;
            *walk = filter_slot->next;

            continue;
        }

        walk = &((*walk)->next);
    }

    if (!filter_slot)
        filter_slot = push_struct(ui_state.view_query.arena, UIFilter);

    *filter_slot = {
        .active = 1,
        .next = NULL,
        .type = FilterType_SizeBetween,
        .val_byte = {
            .from = {.value = 0, .unit = Byte},
            .to = {0},
            .from_enable = 1,
            .to_enable = 0,
        },
    };

    *walk = filter_slot;
}

DBStmtCbk(push_image_metadata)
{
    ui_preview.metadata = {
        .id = sqlite3_column_int64(stmt, 0),

        .atlas_id = sqlite3_column_int64(stmt, 3),
        .atlas_idx = (U32)sqlite3_column_int(stmt, 4),

        .size = size_to_bytesize(sqlite3_column_int64(stmt, 5)),

        .ctime = timestamp_to_time(sqlite3_column_int64(stmt, 6)),
        .mtime = timestamp_to_time(sqlite3_column_int64(stmt, 7)),
        .atime = timestamp_to_time(sqlite3_column_int64(stmt, 8)),

        .width = sqlite3_column_int(stmt, 9),
        .height = sqlite3_column_int(stmt, 10),
        .channels = sqlite3_column_int(stmt, 11),

        .root_dir = sqlite3_column_int64(stmt, 12),
        .parent_dir = sqlite3_column_int64(stmt, 13),
    };
}
