#pragma once
#include "base/base_core.h"
#include "gl/gl_core.h"

#define WIN_WIDTH  1600
#define WIN_HEIGHT 900

struct Window
{
    GLFWwindow *handle;
    F64 begint;
    S32 width, height;
    S32 xpos, ypos;
    B32 active;
};

global_v Window win = {0};

B32 window_init();
void window_poll();
void window_update();
void window_close();
F64 window_deltatime();
