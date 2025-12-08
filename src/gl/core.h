#pragma once

#include <stdio.h>

#include "glfw/glfw3.h"
#include "glad/glad.h"

#define GLCall(x)                                     \
	while (glGetError() != GL_NO_ERROR);              \
	do {                                              \
		GLenum error = glGetError();                  \
		if (error)                                    \
		{                                             \
			printf("[OpenGL Error] (0x%X)\n", error); \
			TRAP();                                   \
		}                                             \
	} while (0)
// TODO: Make a logging system (with colors :) )