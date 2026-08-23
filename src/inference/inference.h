// Copyright (c) 2025-2026 Madhav Goyal
// Licensed under the GNU General Public License v3.0 (see LICENSE)

#pragma once
#include "ortx_tokenizer.h"
#include "base/base_core.h"
#include "base/threadpool.h"

enum InferenceState
{
    InferenceState_Uninitialized,
    InferenceState_Initializing,
    InferenceState_Ready,
    InferenceState_Failed
};

struct TextModelConfig
{
    S64 token_length;
};

struct VisionModelConfig
{
    F32 mean[3];
    F32 std_dev[3];

    S64 input_size;
    F64 rescale_factor;
};

/// Represents a batch of embedding vectors used in the inference engine.
struct Embedding
{
    /// Pointer to the contiguous block of flat embedding data (size: batch_size * dimension).
    F32 *vector;
    /// The size/width of a single embedding vector (e.g., hidden dimension).
    S64 dimension;
    /// The number of individual embedding vectors contained in this batch.
    S64 batch_size;
};

MSCBL_API void inference_init();
MSCBL_API void inference_close();
MSCBL_API InferenceState inference_state_get();
MSCBL_API VisionModelConfig *inference_preprocess_get();
MSCBL_API Embedding inference_text_embedding(Arena *arena, String input);
MSCBL_API Embedding inference_vision_embedding(Arena *arena, F32 *data, U32 batch_size);

ThreadFunc(inference_backend_init);
