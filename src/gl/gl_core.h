#pragma once

#include <stdio.h>

#include "glad/glad.h"
#include "glfw/glfw3.h"

#include "base/base_core.h"

#define GLCall(x)                                     \
	while (glGetError() != GL_NO_ERROR);              \
	do {                                              \
		x;                                            \
		GLenum error = glGetError();                  \
		if (error)                                    \
		{                                             \
			printf("[OpenGL Error] (0x%X)\n", error); \
			TRAP();                                   \
		}                                             \
	} while (0)
// TODO: Make a logging system (with colors :) )

void gl_make_texture(U32 *tex, U8 *data, S32 width, S32 height, S32 channels);