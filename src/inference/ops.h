// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include "inference/inference.h"

F32 ops_vector_magnitude(F32 *vector, S64 size);
void ops_vector_scale(F32 *vector, S64 size, F32 scalar);
void ops_vector_normalize(F32 *vector, S64 size);

inline void ops_embedding_normalize(Embedding embedding)
{
    for (S64 i = 0; i < embedding.batch_size; i++)
        ops_vector_normalize(embedding.vector + (embedding.dimension * i), embedding.dimension);
}
