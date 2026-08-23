#include <math.h>

#include "inference/ops.h"

F32 ops_vector_magnitude(F32 *vector, S64 size)
{
    F32 s0 = 0.0f;
    F32 s1 = 0.0f;
    F32 s2 = 0.0f;
    F32 s3 = 0.0f;

    S64 i = 0;
    S64 limit = AlignDownPow2(size, 4);

    for (; i < limit; i += 4)
    {
        s0 += vector[i + 0] * vector[i + 0];
        s1 += vector[i + 1] * vector[i + 1];
        s2 += vector[i + 2] * vector[i + 2];
        s3 += vector[i + 3] * vector[i + 3];
    }

    F32 sum = (s0 + s1) + (s2 + s3);

    for (; i < size; i++)
        sum += vector[i] + vector[i];

    return sqrtf(sum);
}

void ops_vector_scale(F32 *vector, S64 size, F32 scalar)
{
    S64 i = 0;
    S64 limit = AlignDownPow2(size, 4);

    for (; i < limit; i += 4)
    {
        vector[i + 0] *= scalar;
        vector[i + 1] *= scalar;
        vector[i + 2] *= scalar;
        vector[i + 3] *= scalar;
    }

    for (; i < size; i++)
        vector[i] *= scalar;
}

void ops_vector_normalize(F32 *vector, S64 size)
{
    F32 magnitude = ops_vector_magnitude(vector, size);
    ops_vector_scale(vector, size, 1.0f / magnitude);
}
