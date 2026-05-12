#include "stb_image.h"
#include "base/log.h"
#include "base/string.h"

#include "gl/gl_core.h"

GLState gl_state = {0};

void gl_pop()
{
    if (rb_getsize(gl_state.args))
    {
        EnterCriticalSection(&gl_state.mutex);
        GLArgs arg = rb_pop(gl_state.args);
        LeaveCriticalSection(&gl_state.mutex);

        switch (arg.kind)
        {
        case GLArgs_tex:
            gl_tex_data(arg.v);
            break;
        default:
            break;
        };

        if (arg.wait)
            os_semaphore_drop(arg.sem);
    }
}

void gl_push(GLArgs args, B32 wait)
{
    if (!gl_state.active)
    {
        InitializeCriticalSection(&gl_state.mutex);
        gl_state.active = 1;
    }

    if (wait)
    {
        args.wait = 1;
        args.sem  = os_semaphore_alloc(0, S32_MAX);
    }

    EnterCriticalSection(&gl_state.mutex);
    Assert(rb_push(gl_state.args, args), "ringbuffer is full");
    LeaveCriticalSection(&gl_state.mutex);

    if (wait)
    {
        os_semaphore_take(args.sem, U64_MAX);
        os_semaphore_release(args.sem);
    }
}

void gl_close()
{
    DeleteCriticalSection(&gl_state.mutex);
    gl_state = {0};
}

void gl_tex_data(GLA_tex args)
{
    if (!glIsTexture(*args.texture))
    {
        GLCall(glGenTextures(1, args.texture));
        GLCall(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    }
    GLCall(glBindTexture(GL_TEXTURE_2D, *args.texture));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
    GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));

    GLenum format = GL_RGB, internal_format = GL_RGBA8;
    GLint swizzle_mask[] = {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA};
    switch (args.channels)
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
        Assert(0, "Format definition not found: %d\n", args.channels);
    }

    GLCall(glTexImage2D(GL_TEXTURE_2D, 0, internal_format, args.width, args.height, 0, format, GL_UNSIGNED_BYTE, args.data));
    GLCall(glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask));
}

ThreadFunc(gl_tex_path)
{
    Assert(data.kind == TPData_ANY, "wrong datatype");
    gl_args_path args0 = *(gl_args_path *)data.val_any;

    GLA_tex params1;

    params1.texture = args0.texture;
    params1.data    = stbi_load(CStrCast(args0.path), &params1.width, &params1.height, &params1.channels, 0);
    Assert(params1.data, "image data is NULL (%.*s)", StringSpr(args0.path));

    gl_push({GLArgs_tex, params1});
    stbi_image_free(params1.data);
}
