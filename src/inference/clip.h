#pragma once
// Source: https://github.com/monatis/clip.cpp

#include <string>
#include <map>
#include <vector>

#include "ggml.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"

#include "base/base_core.h"
#include "base/string.h"
#include "base/arena.h"

struct clip_layer
{
    // attention
    ggml_tensor *k_w;
    ggml_tensor *k_b;
    ggml_tensor *q_w;
    ggml_tensor *q_b;
    ggml_tensor *v_w;
    ggml_tensor *v_b;

    ggml_tensor *o_w;
    ggml_tensor *o_b;

    // layernorm 1
    ggml_tensor *ln_1_w;
    ggml_tensor *ln_1_b;

    // ff
    ggml_tensor *ff_i_w;
    ggml_tensor *ff_i_b;

    ggml_tensor *ff_o_w;
    ggml_tensor *ff_o_b;

    // layernorm 2
    ggml_tensor *ln_2_w;
    ggml_tensor *ln_2_b;
};

struct clip_text_hparams
{
    S32 n_vocab;
    S32 num_positions;
    S32 hidden_size;
    S32 n_intermediate;
    S32 projection_dim;
    S32 n_head;
    S32 n_layer;
    F32 eps;
};

struct clip_vision_hparams
{
    S32 image_size;
    S32 patch_size;
    S32 hidden_size;
    S32 n_intermediate;
    S32 projection_dim;
    S32 n_head;
    S32 n_layer;
    F32 eps;
};

struct clip_text_model
{
    struct clip_text_hparams hparams;

    // embeddings
    ggml_tensor *token_embeddings;
    ggml_tensor *position_embeddings;

    clip_layer *layers;

    ggml_tensor *post_ln_w;
    ggml_tensor *post_ln_b;

    ggml_tensor *projection;
    ggml_context *graph_ctx;
    ggml_context *ctx_ggml;
    ggml_backend_t backend;
    ggml_backend_buffer_t backend_buf;
};

struct clip_vision_model
{
    struct clip_vision_hparams hparams;

    // embeddings
    ggml_tensor *class_embedding;
    ggml_tensor *patch_embeddings;
    ggml_tensor *position_embeddings;

    ggml_tensor *pre_ln_w;
    ggml_tensor *pre_ln_b;

    clip_layer *layers;

    ggml_tensor *post_ln_w;
    ggml_tensor *post_ln_b;

    ggml_tensor *projection;
    ggml_context *graph_ctx;
    ggml_context *ctx_ggml;
    ggml_backend_t backend;
    ggml_backend_buffer_t backend_buf;
};

typedef S32 clip_vocab_id;
struct clip_tokens
{
    clip_vocab_id *data;
    U64 size;
};

struct clip_vocab
{
    using id = clip_vocab_id;
    using token = std::string;

    std::map<token, id> token_to_id;
    std::map<id, token> id_to_token;
    std::vector<std::string> special_tokens;

    void add_special_token(const std::string &token);
};

struct clip_ctx
{
    bool has_text_encoder = false;
    bool has_vision_encoder = false;
    clip_text_model text_model;
    clip_vision_model vision_model;
    clip_vocab vocab;
    F32 image_mean[3];
    F32 image_std[3];
    bool use_gelu = false;
    S32 ftype = 1;
};

struct Embedding
{
    F32 *vector;
    S32 size;
    S32 count;
};

void clip_model_load(Arena *arena, clip_ctx *clip, const char *fname);

void clip_free(clip_ctx *ctx);

// TODO: Assuming this works **as intended** for now. Need the check later
bool clip_tokenize(clip_ctx *ctx, String *input, struct clip_tokens *tokens);

Embedding clip_get_text_embedding(Arena *arena, clip_ctx *clip, clip_tokens *tokens);
ggml_cgraph *build_text_encode_graph(clip_text_model *text_model, clip_ctx *clip, U64 token_size);

Embedding clip_get_image_embedding(Arena *arena, clip_ctx *clip, ggml_cgraph *graph, F32 *imageData, S32 batch_size);
ggml_cgraph *build_image_encode_graph(clip_vision_model *vision_model, clip_ctx *clip, S32 batch_size);

// bool image_normalize(const clip_image_u8 *img, clip_image_f32 *res);

// bool clip_compare_text_and_image(struct clip_ctx *ctx, const int n_threads,
//                                  const char *text,
//                                  const struct clip_image_u8 *image,
//                                  F32 *score);
// F32 clip_similarity_score(const F32 *vec1, const F32 *vec2,
//                             const int vec_dim);
bool softmax_with_sorting(F32 *arr, const int length, F32 *sorted_scores, int *indices);
// bool clip_zero_shot_label_image(struct clip_ctx *ctx, const int n_threads,
//                                 const struct clip_image_u8 *input_img,
//                                 const char **labels, const U64 n_labels,
//                                 F32 *scores, int *indices);

bool clip_model_quantize(const char *fname_inp, const char *fname_out, const int itype);
