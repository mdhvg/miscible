#pragma once

#include "base/base_core.h"
#include "gl/gl_core.h"

#define WIN_WIDTH  1280
#define WIN_HEIGHT 720

struct Window
{
	GLFWwindow *handle;
	F64 begint;
	S32 width, height;
	S32 xpos, ypos;
	S8 active;
};

global Window win = {0};

B8 window_init();
void window_poll();
void window_update();
void window_close();
F64 window_get_deltatime();

// class Window
// {
//   public:
// 	bool init();
// 	void close();

// 	inline GLFWwindow *get_handle()
// 	{
// 		return win;
// 	}
// 	inline bool is_open()
// 	{
// 		return !glfwWindowShouldClose(win);
// 	}
// 	inline ImVec2 get_pos()
// 	{
// 		return {(float)xpos, (float)ypos};
// 	}
// 	inline ImVec2 get_size()
// 	{
// 		return {(float)xpos, (float)ypos};
// 	}
// 	inline double get_delta()
// 	{
// 		double cur = glfwGetTime();
// 		double delta = cur - start_time;
// 		start_time = cur;
// 		return delta;
// 	}
// 	void poll();
// 	void update();

//   private:
// 	GLFWwindow *win{NULL};
// 	int width, height;
// 	int xpos, ypos;
// 	double start_time;
// };