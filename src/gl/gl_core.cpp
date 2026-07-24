// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#include "GLFW/glfw3.h"
#include "stb_image.h"

#include "gl/gl_core.h"
#include "base/log.h"
#include "base/string.h"
#include "window/window.h"
#include "base/threadpool.h"

void gl_tex_data(U32 *texture, U8 *data, S32 width, S32 height, S32 channels, B32 free)
{
    os_mutex_lock(&win.upload_mutex);
    glfwMakeContextCurrent(win.upload_handle);
    if (!glIsTexture(*texture))
    {
        GLCall(glGenTextures(1, texture));
        GLCall(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    }
    GLCall(glBindTexture(GL_TEXTURE_2D, *texture));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));

    GLenum format = GL_RGB, internal_format = GL_RGBA8;
    GLint swizzle_mask[] = {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA};
    switch (channels)
    {
    case 1:
        format = GL_RED;
        internal_format = GL_R8;
        swizzle_mask[1] = GL_RED;
        swizzle_mask[2] = GL_RED;
        swizzle_mask[3] = GL_ONE;
        break;
    case 2:
        format = GL_RG;
        internal_format = GL_RG8;
        swizzle_mask[1] = GL_RED;
        swizzle_mask[2] = GL_RED;
        swizzle_mask[3] = GL_GREEN;
        break;
    case 3:
        format = GL_RGB;
        internal_format = GL_RGB8;
        swizzle_mask[3] = GL_ONE;
        break;
    case 4:
        format = GL_RGBA;
        internal_format = GL_RGBA8;
        break;
    default:
        Assert(0, "Format definition not found: %d\n", channels);
    }

    GLCall(glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, data));
    GLCall(glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask));
    glfwMakeContextCurrent(NULL);
    os_mutex_unlock(&win.upload_mutex);

    if (free)
        stbi_image_free(data);
}

MSCBL_API ThreadFunc(gl_tex_path)
{
    Assert(args[0].kind == TPData_Any, "invalid datatype");
    Assert(args[1].kind == TPData_String, "invalid datatype");

    S32 width, height, channels;
    U8 *data = stbi_load(CStrCast(args[1].val_str), &width, &height, &channels, 0);
    Assert(data, "image data is NULL (%.*s)", StringSpr(args[1].val_str));
    gl_tex_data((U32 *)args[0].val_any, data, width, height, channels, true);
}

MSCBL_API ThreadFunc(gl_tex_mem)
{
    Assert(args[0].kind == TPData_Any, "invalid datatype");
    Assert(args[1].kind == TPData_Any, "invalid datatype");
    Assert(args[2].kind == TPData_U64, "invalid datatype");

    S32 width, height, channels;
    U8 *data = stbi_load_from_memory((U8 *)args[1].val_any, args[2].val_u64, &width, &height, &channels, 4);
    Assert(data, "image data is NULL");
    gl_tex_data((U32 *)args[0].val_any, data, width, height, channels, true);
}
