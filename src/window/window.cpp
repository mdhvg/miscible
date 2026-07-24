// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include "gl/gl_core.h"
#include "ui/theme.h"
#include "base/log.h"
#include "os/os_inc.h"
#include "ui/ui_core.h"
#include "window/window.h"

Window win = {.handle = 0};
local_v B32 window_dragging = 0;
local_v F32 drag_start_x = 0.0f;
local_v F32 drag_start_y = 0.0f;
#define TITLEBAR_HEIGHT SPACING(6)

local_v void glfw_error_callback(S32 error, const char *description)
{
    mscbl_log_error("0x%X: %s", error, description);
}

local_v void win_close_callback(GLFWwindow *handle)
{
    win.active = 0;
}

local_v void mouse_btn_cbk(GLFWwindow *window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);

            if (mouseY >= 0 && mouseY <= TITLEBAR_HEIGHT)
            {
                window_dragging = true;
                drag_start_x = mouseX;
                drag_start_y = mouseY;
            }
        }
        else if (action == GLFW_RELEASE)
        {
            window_dragging = false;
        }
    }
}

void cursor_pos_cbk(GLFWwindow *window, double xpos, double ypos)
{
    if (window_dragging)
    {
        S32 win_x = 0, win_y = 0;
        glfwGetWindowPos(window, &win_x, &win_y);

        S32 new_x = win_x + (S32)(xpos - drag_start_x);
        S32 new_y = win_y + (S32)(ypos - drag_start_y);

        glfwSetWindowPos(window, new_x, new_y);
    }
}

void window_init()
{
    glfwSetErrorCallback(glfw_error_callback);
    Assert(glfwInit(), "Failed to initialize GLFW");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DECORATED, GL_FALSE);

    win.handle = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, Stringify(APP_NAME), NULL, NULL);
    Assert(win.handle, "GLFW window creation failed");
    glfwSetWindowCloseCallback(win.handle, win_close_callback);

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    win.upload_handle = glfwCreateWindow(1, 1, "Background", NULL, win.handle);
    Assert(win.upload_handle, "GLFW window creation failed");
    os_mutex_init(&win.upload_mutex);

    glfwMakeContextCurrent(NULL);
    glfwMakeContextCurrent(win.handle);
    glfwSwapInterval(1); // Enable vsync

    glfwSetMouseButtonCallback(win.handle, mouse_btn_cbk);
    glfwSetCursorPosCallback(win.handle, cursor_pos_cbk);

    // TODO: Switch to native window init for win32
#if OS_WIN32
    HWND hwnd = glfwGetWin32Window(win.handle);
    HINSTANCE hInst = GetModuleHandle(NULL);
    HICON hAppIcon = LoadIcon(hInst, MAKEINTRESOURCE(1));

    if (hAppIcon)
    {
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hAppIcon);
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hAppIcon);
    }
    else
    {
        mscbl_log_error("Failed to load icon resource ID 1. Windows Error: %lu", (GetLastError()));
    }
#endif

    Assert(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress), "Failed to initialize OpenGL loader!");
    win.begint = glfwGetTime();

    mscbl_log_info("OpenGL Version: %s", glGetString(GL_VERSION));
    mscbl_log_info("GLSL Version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
    mscbl_log_info("GPU Vendor: %s", glGetString(GL_VENDOR));
    mscbl_log_info("Renderer: %s", glGetString(GL_RENDERER));
    S32 maxTextureUnits = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
    mscbl_log_info("Maximum texture units: %d", maxTextureUnits);
    S32 maxLayers;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
    mscbl_log_info("Maximum texture array layers supported: %d", maxLayers);
    win.active = 1;
}

B32 window_poll()
{
    if (glfwGetWindowAttrib(win.handle, GLFW_ICONIFIED))
    {
        glfwWaitEvents();
        return 0;
    }
    if (ui_needs_update())
        glfwWaitEventsTimeout(1.0f / 30.0f);
    else
        glfwWaitEvents();
    glfwGetWindowSize(win.handle, &win.width, &win.height);
    glfwGetWindowPos(win.handle, &win.xpos, &win.ypos);
    GLCall(glViewport(win.xpos, win.ypos, win.width, win.height));
    glClearColor(1, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    return 1;
}

void window_update()
{
#if OS_WIN32
    // NOTE: I have no idea why it works, but without it, CPU usage
    // in Windows explodes
    // Reference: https://stackoverflow.com/a/63540019
    win32_sleep_ms(1);
#endif
    // glfwSetWindowTitle(win.handle, str_to_cstr(string_format(persistent_arena, APP_NAME " FPS: %.2f", 1.0f / window_get_deltatime())));
    GLCall(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    glfwMakeContextCurrent(win.handle);
    glfwSwapBuffers(win.handle);
}

void window_iconify()
{
    glfwIconifyWindow(win.handle);
}

void window_maximize()
{
    glfwMaximizeWindow(win.handle);
    win.maximized = 1;
}

void window_minimize()
{
    glfwRestoreWindow(win.handle);
    win.maximized = 0;
}

void window_close()
{
    glfwSetWindowShouldClose(win.handle, GLFW_TRUE);
    win.active = 0;
}

void window_shutdown()
{
    glfwDestroyWindow(win.handle);
    glfwDestroyWindow(win.upload_handle);
    os_mutex_destroy(&win.upload_mutex);
    glfwTerminate();
}

F64 window_deltatime()
{
    F64 now = glfwGetTime();
    F64 res = now - win.begint;
    win.begint = now;
    return res;
}
