#pragma once
#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include "base/log.h"
#include "base/string.h"

#define GLCall(x)                                     \
    while (glGetError() != GL_NO_ERROR);              \
    do                                                \
    {                                                 \
        x;                                            \
        GLenum error = glGetError();                  \
        if (error)                                    \
        {                                             \
            mscbl_log_error(OpenGL, "(0x%X)", error); \
            TRAP();                                   \
        }                                             \
    } while (0)

void gl_make_texture(U32 *tex, U8 *data, S32 width, S32 height, S32 channels);
MSCBL_API void gl_make_texture(U32 *texture, String path);
