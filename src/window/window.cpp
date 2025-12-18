#include "window/window.h"
#include "GLFW/glfw3.h"
#include "base/base_core.h"
#include "gl/gl_core.h"
#include "window.h"

internal void glfw_error_callback(S32 error, const char *description)
{
	printf("GLFW Error 0x%X: %s\n", error, description);
}

internal void win_close_callback(GLFWwindow *handle)
{
	win.active = 0;
}

B8 window_init()
{
	glfwSetErrorCallback(glfw_error_callback);
	Assert(glfwInit() && "Failed to initialize GLFW");

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	win.handle = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, APP_NAME, NULL, NULL);
	if (!win.handle) return 0;
	glfwSetWindowCloseCallback(win.handle, win_close_callback);
	glfwMakeContextCurrent(win.handle);
	glfwSwapInterval(1); // Enable vsync

	Assert(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) && "Failed to initialize OpenGL loader!");
	win.begint = glfwGetTime();

	printf("OpenGL Version: %s\n", (char *)glGetString(GL_VERSION));
	printf("GLSL Version: %s\n", (char *)glGetString(GL_SHADING_LANGUAGE_VERSION));
	printf("GPU Vendor: %s\n", (char *)glGetString(GL_VENDOR));
	printf("Renderer: %s\n", (char *)glGetString(GL_RENDERER));
	S32 maxTextureUnits = 0;
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
	printf("Maximum texture units: %d\n", maxTextureUnits);
	S32 maxLayers;
	glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers);
	printf("Maximum texture array layers supported: %d\n", maxLayers);
	win.active = 1;
	return 1;
}

void window_poll()
{
	glfwPollEvents();
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

F64 window_get_deltatime()
{
	F64 now	   = glfwGetTime();
	F64 res	   = now - win.begint;
	win.begint = now;
	return res;
}

// bool Window::init()
// {
// }

// void Window::close()
// {
// 	glfwDestroyWindow(win);
// 	glfwTerminate();
// }

// void Window::poll()
// {
// }

// void Window::update()
// {
// 	GLCall(glBindFramebuffer(GL_FRAMEBUFFER, 0));
// 	glfwMakeContextCurrent(win);
// 	glfwSwapBuffers(win);
// }
