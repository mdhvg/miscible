#include "window/window.h"
#include "base/log.h"
#include "gl/gl_core.h"
#include "ui/ui_core.h"

local_v void glfw_error_callback(S32 error, const char *description)
{
    mscbl_log_error("0x%X: %s", error, description);
}

local_v void win_close_callback(GLFWwindow *handle)
{
    win.active = 0;
}

B32 window_init()
{
    glfwSetErrorCallback(glfw_error_callback);
    Assert(glfwInit(), "Failed to initialize GLFW");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    win.handle = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, Stringify(APP_NAME), NULL, NULL);
    if (!win.handle) return 0;
    glfwSetWindowCloseCallback(win.handle, win_close_callback);
    glfwMakeContextCurrent(win.handle);
    glfwSwapInterval(1); // Enable vsync

    Assert(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress), "Failed to initialize OpenGL loader!");
    win.begint = glfwGetTime();

    mscbl_log_dbg("OpenGL Version: %s", glGetString(GL_VERSION));
    mscbl_log_dbg("GLSL Version: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
    mscbl_log_dbg("GPU Vendor: %s", glGetString(GL_VENDOR));
    mscbl_log_dbg("Renderer: %s", glGetString(GL_RENDERER));
    S32 maxTextureUnits = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
    mscbl_log_dbg("Maximum texture units: %d", maxTextureUnits);
    S32 maxLayers;
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
    mscbl_log_dbg("Maximum texture array layers supported: %d", maxLayers);
    win.active = 1;
    return 1;
}

void window_poll()
{
    if (ui_needs_update())
        glfwWaitEventsTimeout(1.0f / 30.0f);
    else
        glfwWaitEvents();
    // glfwPollEvents();
    glfwGetWindowSize(win.handle, &win.width, &win.height);
    glfwGetWindowPos(win.handle, &win.xpos, &win.ypos);
    GLCall(glViewport(win.xpos, win.ypos, win.width, win.height));
    glClearColor(1, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
}

void window_update()
{
#if OS_WINDOWS
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

void window_close()
{
    glfwDestroyWindow(win.handle);
    glfwTerminate();
}

F64 window_deltatime()
{
    F64 now = glfwGetTime();
    F64 res = now - win.begint;
    win.begint = now;
    return res;
}
