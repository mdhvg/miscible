#pragma once
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "base/ringbuf.h"
#include "base/threadpool.h"

#include "base/log.h"
#include "base/string.h"

#define GLCall(x)                                     \
    while (glGetError() != GL_NO_ERROR);              \
    do                                                \
    {                                                 \
        x;                                            \
        GLenum error = glGetError();                  \
        Assert(!error, "OpenGL error (0x%X)", error); \
    } while (0)

enum GLArgs_
{
    GLArgs_NONE,
    GLArgs_tex,
    GLArgs_COUNT,
};

struct GLA_tex
{
    U32 *texture;
    U8 *data;
    S32 width;
    S32 height;
    S32 channels;
};

struct GLArgs
{
    GLArgs_ kind;
    union {
        GLA_tex v;
    };
    B32 wait;
    Semaphore sem;
};

struct GLState
{
    B32 active;
    Mutex mutex;
    RingBuffer(GLArgs, args, KB(4));
};

void gl_push(GLArgs args, B32 wait = 1);
void gl_pop();
void gl_close();

struct gl_args_path
{
    U32 *texture;
    String path;
};

MSCBL_API void gl_tex_data(GLA_tex args);
MSCBL_API ThreadFunc(gl_tex_path);
