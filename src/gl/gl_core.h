// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "base/threadpool.h"

#define GLCall(x)                                     \
    while (glGetError() != GL_NO_ERROR);              \
    do                                                \
    {                                                 \
        x;                                            \
        GLenum error = glGetError();                  \
        Assert(!error, "OpenGL error (0x%X)", error); \
    } while (0)

void gl_tex_data(U32 *texture, U8 *data, S32 width, S32 height, S32 channels);
MSCBL_API ThreadFunc(gl_tex_id);
MSCBL_API ThreadFunc(gl_tex_path);
MSCBL_API ThreadFunc(gl_tex_mem);
