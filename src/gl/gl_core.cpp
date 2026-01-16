#include "gl/gl_core.h"
#include "base/log.h"

void gl_make_texture(U32 *tex, U8 *data, S32 width, S32 height, S32 channels)
{
    if (!tex || !glIsTexture(*tex))
    {
        GLCall(glGenTextures(1, tex));
        GLCall(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    }
    GLCall(glBindTexture(GL_TEXTURE_2D, *tex));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));

    GLenum format = GL_RGB, internal_format = GL_RGBA8;
    GLint swizzle_mask[] = {GL_RED, GL_GREEN, GL_BLUE, GL_ONE};
    switch (channels)
    {
    case 1:
        format          = GL_RED;
        internal_format = GL_R8;
        swizzle_mask[1] = GL_RED;
        swizzle_mask[2] = GL_RED;
        swizzle_mask[3] = GL_ONE;
        break;
    case 2:
        format          = GL_RG;
        internal_format = GL_RG8;
        swizzle_mask[2] = GL_RED;
        swizzle_mask[3] = GL_ONE;
        break;
    case 3:
        format          = GL_RGB;
        internal_format = GL_RGB8;
        swizzle_mask[3] = GL_ONE;
        break;
    case 4:
        format          = GL_RGBA;
        internal_format = GL_RGBA8;
        break;
    default:
        mscbl_log_error(gl_make_texture, "Format definition not found: %d\n", channels);
        Assert(false);
    }

    GLCall(glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, data));
    GLCall(glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask));
}
