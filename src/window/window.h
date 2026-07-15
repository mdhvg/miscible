#pragma once
#include "os/os_inc.h"
#include "GLFW/glfw3.h"
#include "base/base_core.h"

#define WIN_WIDTH  1600
#define WIN_HEIGHT 900

struct Window
{
    GLFWwindow *handle;
    Mutex upload_mutex;
    GLFWwindow *upload_handle;
    F64 begint;
    S32 width, height;
    S32 xpos, ypos;
    B32 active;

    B32 maximized;
};

MSCBL_API Window win;

void window_init();
B32 window_poll();
void window_update();

void window_iconify();
void window_maximize();
void window_minimize();
void window_close();

void window_shutdonw();
F64 window_deltatime();
