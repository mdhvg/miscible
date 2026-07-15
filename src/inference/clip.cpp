#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <cmath>
#include <regex>
#include <vector>

#include "clip.h"
#include "base/arena.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"

#include "ggml.h"
#include "gguf.h"
#include "base/log.h"
#include "inference/clip.h"
#include "base/base_core.h"

//
// key constants
//

#define KEY_FTYPE          "general.file_type"
#define KEY_NAME           "general.name"
#define KEY_DESCRIPTION    "general.description"
#define KEY_HAS_TEXT_ENC   "clip.has_text_encoder"
#define KEY_HAS_VIS_ENC    "clip.has_vision_encoder"
#define KEY_USE_GELU       "clip.use_gelu"
#define KEY_N_EMBD         "clip.%s.embedding_length"
#define KEY_N_FF           "clip.%s.feed_forward_length"
#define KEY_N_BLOCK        "clip.%s.block_count"
#define KEY_N_HEAD         "clip.%s.attention.head_count"
#define KEY_LAYER_NORM_EPS "clip.%s.attention.layer_norm_epsilon"
#define KEY_PROJ_DIM       "clip.%s.projection_dim"
#define KEY_TOKENS         "tokenizer.ggml.tokens"
#define KEY_N_POSITIONS    "clip.text.context_length"
#define KEY_IMAGE_SIZE     "clip.vision.image_size"
#define KEY_PATCH_SIZE     "clip.vision.patch_size"
#define KEY_IMAGE_MEAN     "clip.vision.image_mean"
#define KEY_IMAGE_STD      "clip.vision.image_std"

//
// tensor name constants
//

#define TN_TOKEN_EMBD  "%s.token_embd.weight"
#define TN_POS_EMBD    "%s.position_embd.weight"
#define TN_CLASS_EMBD  "v.class_embd"
#define TN_PATCH_EMBD  "v.patch_embd.weight"
#define TN_ATTN_K      "%s.blk.%d.attn_k.%s"
#define TN_ATTN_Q      "%s.blk.%d.attn_q.%s"
#define TN_ATTN_V      "%s.blk.%d.attn_v.%s"
#define TN_ATTN_OUTPUT "%s.blk.%d.attn_out.%s"
#define TN_FFN_DOWN    "%s.blk.%d.ffn_down.%s"
#define TN_FFN_UP      "%s.blk.%d.ffn_up.%s"
#define TN_LN_1        "%s.blk.%d.ln1.%s"
#define TN_LN_2        "%s.blk.%d.ln2.%s"
#define TN_LN_PRE      "%s.pre_ln.%s"
#define TN_LN_POST     "%s.post_ln.%s"
#define TN_TEXT_PROJ   "text_projection.weight"
#define TN_VIS_PROJ    "visual_projection.weight"

//
// utilities to get data from a gguf file
//

S32 get_key_idx(const gguf_context *ctx, const char *key)
{
    S32 i = gguf_find_key(ctx, key);
    Assert(i != -1, "key not found");
    return i;
}

uint32_t get_u32(const gguf_context *ctx, std::string key)
{
    const S32 i = get_key_idx(ctx, key.c_str());

    return gguf_get_val_u32(ctx, i);
}

float get_f32(const gguf_context *ctx, std::string key)
{
    const S32 i = get_key_idx(ctx, key.c_str());

    return gguf_get_val_f32(ctx, i);
}

ggml_tensor *get_tensor(ggml_context *ctx, std::string name)
{
    ggml_tensor *cur = ggml_get_tensor(ctx, name.c_str());
    Assert(cur, "unable to find tensor %s", name.c_str());
    return cur;
}

const char *get_ftype(S32 ftype)
{
    switch (ftype)
    {
    case 0:
        return "f32";
        break;
    case 1:
        return "f16";
        break;
    case 2:
        return "q4_0";
        break;
    case 3:
        return "q4_1";
        break;
    case 6:
        return "q5_0";
        break;
    case 7:
        return "q5_1";
        break;
    case 8:
        return "q8_0";
        break;
    default:
        Assert(0, "Unrecognized file type: %d\n", ftype);
        return NULL;
    }
}

void clip_load_text_model(Arena *arena, clip_ctx *clip, clip_text_model *text_model, ggml_context *temp_ctx, gguf_context *gguf_ctx)
{
    ggml_context *model_ctx = text_model->ctx_ggml;
    text_model->backend = ggml_backend_init_best();
    Assert(text_model->backend, "ggml_backend_init_best failed");
    text_model->backend_buf = ggml_backend_alloc_ctx_tensors(model_ctx, text_model->backend);

    for (ggml_tensor *cur = ggml_get_first_tensor(model_ctx);
         cur != NULL;
         cur = ggml_get_next_tensor(model_ctx, cur))
    {
        ggml_tensor *src = ggml_get_tensor(temp_ctx, ggml_get_name(cur));
        size_t n_size = ggml_nbytes(src);
        ggml_backend_tensor_set(cur, ggml_get_data(src), 0, n_size);
    }

    StringBuilder keys = string_empty(arena, 256);

    clip_text_hparams *hparams = &text_model->hparams;
    hparams->hidden_size = get_u32(gguf_ctx, format_cstr(&keys, KEY_N_EMBD, "text"));
    hparams->n_head = get_u32(gguf_ctx, format_cstr(&keys, KEY_N_HEAD, "text"));
    hparams->n_intermediate = get_u32(gguf_ctx, format_cstr(&keys, KEY_N_FF, "text"));
    hparams->n_layer = get_u32(gguf_ctx, format_cstr(&keys, KEY_N_BLOCK, "text"));
    hparams->num_positions = get_u32(gguf_ctx, KEY_N_POSITIONS);
    hparams->projection_dim = get_u32(gguf_ctx, format_cstr(&keys, KEY_PROJ_DIM, "text"));
    hparams->eps = get_f32(gguf_ctx, format_cstr(&keys, KEY_LAYER_NORM_EPS, "text"));

    S32 idx_tokens = get_key_idx(gguf_ctx, KEY_TOKENS);
    hparams->n_vocab = gguf_get_arr_n(gguf_ctx, idx_tokens);
    clip_vocab *vocab = &clip->vocab;
    // TODO: This needs to go but before this replace std::string and std::vector
    // in `clip_vocab` with custom containers
    new (&clip->vocab) clip_vocab();

    for (S32 id = 0; id < hparams->n_vocab; ++id)
    {
        const char *token = gguf_get_arr_str(gguf_ctx, idx_tokens, id);
        vocab->id_to_token[id] = token;
        vocab->token_to_id[token] = id;
    }

    mscbl_log_info("text model hparams");
    mscbl_log_info("n_vocab            %d", hparams->n_vocab);
    mscbl_log_info("num_positions      %d", hparams->num_positions);
    mscbl_log_info("t_hidden_size      %d", hparams->hidden_size);
    mscbl_log_info("t_n_intermediate   %d", hparams->n_intermediate);
    mscbl_log_info("t_projection_dim   %d", hparams->projection_dim);
    mscbl_log_info("t_n_head           %d", hparams->n_head);
    mscbl_log_info("t_n_layer          %d", hparams->n_layer);

    text_model->token_embeddings = get_tensor(model_ctx, format_cstr(&keys, TN_TOKEN_EMBD, "t"));
    text_model->position_embeddings = get_tensor(model_ctx, format_cstr(&keys, TN_POS_EMBD, "t"));
    text_model->post_ln_w = get_tensor(model_ctx, format_cstr(&keys, TN_LN_POST, "t", "weight"));
    text_model->post_ln_b = get_tensor(model_ctx, format_cstr(&keys, TN_LN_POST, "t", "bias"));
    text_model->projection = get_tensor(model_ctx, TN_TEXT_PROJ);
    text_model->layers = push_array(arena, hparams->n_layer, clip_layer);

    for (S32 il = 0; il < hparams->n_layer; ++il)
    {
        clip_layer *layer = text_model->layers + il;
        layer->k_w = get_tensor(model_ctx, format_cstr(&keys, TN_ATTN_K, "t", il, "weight"));
        layer->q_w = get_tensor(model_ctx, format_cstr(&keys, TN_ATTN_Q, "t", il, "weight"));
        layer->v_w = get_tensor(model_ctx, format_cstr(&keys, TN_ATTN_V, "t", il, "weight"));
        layer->o_w = get_tensor(model_ctx, format_cstr(&keys, TN_ATTN_OUTPUT, "t", il, "weight"));
        layer->ln_1_w = get_tensor(model_ctx, format_cstr(&keys, TN_LN_1, "t", il, "weight"));
        layer->ln_2_w = get_tensor(model_ctx, format_cstr(&keys, TN_LN_2, "t", il, "weight"));
        layer->ff_i_w = get_tensor(model_ctx, format_cstr(&keys, TN_FFN_DOWN, "t", il, "weight"));
        layer->ff_o_w = get_tensor(model_ctx, format_cstr(&keys, TN_FFN_UP, "t", il, "weight"));
        layer->k_b = get_tensor(model_ctx, format_cstr(&keys, TN_ATTN_K, "t", il, "bias"));
        layer->q_b = get_tensor(model_ctx, format_cstr(&keys, TN_ATTN_Q, "t", il, "bias"));
        layer->v_b = get_tensor(model_ctx, format_cstr(&keys, TN_ATTN_V, "t", il, "bias"));
        layer->o_b = get_tensor(model_ctx, format_cstr(&keys, TN_ATTN_OUTPUT, "t", il, "bias"));
        layer->ln_1_b = get_tensor(model_ctx, format_cstr(&keys, TN_LN_1, "t", il, "bias"));
        layer->ln_2_b = get_tensor(model_ctx, format_cstr(&keys, TN_LN_2, "t", il, "bias"));
        layer->ff_i_b = get_tensor(model_ctx, format_cstr(&keys, TN_FFN_DOWN, "t", il, "bias"));
        layer->ff_o_b = get_tensor(model_ctx, format_cstr(&keys, TN_FFN_UP, "t", il, "bias"));
    }

    mscbl_log_info("Used mem: %zu", ggml_used_mem(model_ctx));
    mscbl_log_info("Backend Size: %zu", ggml_backend_buffer_get_size(text_model->backend_buf));

    U64 mem_size = ggml_tensor_overhead() * GGML_DEFAULT_GRAPH_SIZE + ggml_graph_overhead();
    text_model->graph_ctx = ggml_init({.mem_size = mem_size,
                                       .mem_buffer = arena_push(arena, mem_size, 0, GGML_MEM_ALIGN),
                                       .no_alloc = true});
}

void clip_load_vision_model(Arena *arena, clip_ctx *clip, clip_vision_model *vision_model, ggml_context *temp_ctx, gguf_context *gguf_ctx)
{
    ggml_context *model_ctx = vision_model->ctx_ggml;
    vision_model->backend = ggml_backend_init_best();
    Assert(vision_model->backend, "ggml_backend_init_best failed");
    vision_model->backend_buf = ggml_backend_alloc_ctx_tensors(vision_model->ctx_ggml, vision_model->backend);

    for (ggml_tensor *cur = ggml_get_first_tensor(vision_model->ctx_ggml);
         cur != NULL;
         cur = ggml_get_next_tensor(vision_model->ctx_ggml, cur))
    {
        ggml_tensor *src = ggml_get_tensor(temp_ctx, ggml_get_name(cur));
        size_t n_size = ggml_nbytes(src);
        ggml_backend_tensor_set(cur, ggml_get_data(src), 0, n_size);
    }

    StringBuilder keys = string_empty(arena, 256);

    clip_vision_hparams *hparams = &vision_model->hparams;
    hparams->hidden_size = get_u32(gguf_ctx, format_cstr(&keys, KEY_N_EMBD, "vision"));
    hparams->n_head = get_u32(gguf_ctx, format_cstr(&keys, KEY_N_HEAD, "vision"));
    hparams->n_intermediate = get_u32(gguf_ctx, format_cstr(&keys, KEY_N_FF, "vision"));
    hparams->n_layer = get_u32(gguf_ctx, format_cstr(&keys, KEY_N_BLOCK, "vision"));
    hparams->image_size = get_u32(gguf_ctx, KEY_IMAGE_SIZE);
    hparams->patch_size = get_u32(gguf_ctx, KEY_PATCH_SIZE);
    hparams->projection_dim = get_u32(gguf_ctx, format_cstr(&keys, KEY_PROJ_DIM, "vision"));
    hparams->eps = get_f32(gguf_ctx, format_cstr(&keys, KEY_LAYER_NORM_EPS, "vision"));

    S32 idx_mean = get_key_idx(gguf_ctx, KEY_IMAGE_MEAN);
    S32 idx_std = get_key_idx(gguf_ctx, KEY_IMAGE_STD);
    for (S32 i = 0; i < 3; ++i)
    {
        clip->image_mean[i] = ((F32 *)gguf_get_arr_data(gguf_ctx, idx_mean))[i];
        clip->image_std[i] = ((F32 *)gguf_get_arr_data(gguf_ctx, idx_std))[i];
    }

    mscbl_log_info("vision model hparams");
    mscbl_log_info("image_size         %d", hparams->image_size);
    mscbl_log_info("patch_size         %d", hparams->patch_size);
    mscbl_log_info("v_hidden_size      %d", hparams->hidden_size);
    mscbl_log_info("v_n_intermediate   %d", hparams->n_intermediate);
    mscbl_log_info("v_projection_dim   %d", hparams->projection_dim);
    mscbl_log_info("v_n_head           %d", hparams->n_head);
    mscbl_log_info("v_n_layer          %d", hparams->n_layer);

    vision_model->patch_embeddings = get_tensor(model_ctx, TN_PATCH_EMBD);
    vision_model->class_embedding = get_tensor(model_ctx, TN_CLASS_EMBD);
    vision_model->position_embeddings = get_tensor(model_ctx, format_cstr(&keys, TN_POS_EMBD, "v"));
    vision_model->pre_ln_w = get_tensor(model_ctx, format_cstr(&keys, TN_LN_PRE, "v", "weight"));
    vision_model->pre_ln_b = get_tensor(model_ctx, format_cstr(&keys, TN_LN_PRE, "v", "bias"));
    vision_model->post_ln_w = get_tensor(model_ctx, format_cstr(&keys, TN_LN_POST, "v", "weight"));
    vision_model->post_ln_b = get_tensor(model_ctx, format_cstr(&keys, TN_LN_POST, "v", "bias"));
    vision_model->projection = get_tensor(model_ctx, TN_VIS_PROJ);
    vision_model->layers = push_array(arena, hparams->n_layer, clip_layer);

    for (S32 il = 0; il < hparams->n_layer; ++il)
    {
        clip_layer *layer = vision_model->layers + il;
        layer->k_w = get_tensor(model_ctx, format_cstr(&keys, TN_ATTN_K, "v", il, "weight"));
        layer->q_w = get_tensor(model_ctx, format_cstr(&keys, TN_ATTN_Q, "v", il, "weight"));
        layer->v_w = get_tensor(model_ctx, format_cstr(&keys, TN_ATTN_V, "v", il, "weight"));
        layer->o_w = get_tensor(model_ctx, format_cstr(&keys, TN_ATTN_OUTPUT, "v", il, "weight"));
        layer->ln_1_w = get_tensor(model_ctx, format_cstr(&keys, TN_LN_1, "v", il, "weight"));
        layer->ln_2_w = get_tensor(model_ctx, format_cstr(&keys, TN_LN_2, "v", il, "weight"));
        layer->ff_i_w = get_tensor(model_ctx, format_cstr(&keys, TN_FFN_DOWN, "v", il, "weight"));
        layer->ff_o_w = get_tensor(model_ctx, format_cstr(&keys, TN_FFN_UP, "v", il, "weight"));
        layer->k_b = get_tensor(model_ctx, format_cstr(&keys, TN_ATTN_K, "v", il, "bias"));
        layer->q_b = get_tensor(model_ctx, format_cstr(&keys, TN_ATTN_Q, "v", il, "bias"));
        layer->v_b = get_tensor(model_ctx, format_cstr(&keys, TN_ATTN_V, "v", il, "bias"));
        layer->o_b = get_tensor(model_ctx, format_cstr(&keys, TN_ATTN_OUTPUT, "v", il, "bias"));
        layer->ln_1_b = get_tensor(model_ctx, format_cstr(&keys, TN_LN_1, "v", il, "bias"));
        layer->ln_2_b = get_tensor(model_ctx, format_cstr(&keys, TN_LN_2, "v", il, "bias"));
        layer->ff_i_b = get_tensor(model_ctx, format_cstr(&keys, TN_FFN_DOWN, "v", il, "bias"));
        layer->ff_o_b = get_tensor(model_ctx, format_cstr(&keys, TN_FFN_UP, "v", il, "bias"));
    }

    mscbl_log_info("Used mem: %zu", ggml_used_mem(model_ctx));
    mscbl_log_info("Backend Size: %zu", ggml_backend_buffer_get_size(vision_model->backend_buf));

    U64 mem_size = ggml_tensor_overhead() * GGML_DEFAULT_GRAPH_SIZE + ggml_graph_overhead();
    vision_model->graph_ctx = ggml_init({.mem_size = mem_size,
                                         .mem_buffer = arena_push(arena, mem_size, 0, GGML_MEM_ALIGN),
                                         .no_alloc = true});
}

// read and create ggml_context containing the tensors and their data
Result clip_model_load(Arena *arena, clip_ctx *clip, String model_path)
{
    Result res = ResultSuccess();

    S32 n_tensors = 0, n_kv = 0, ftype = 0, idx_desc = 0, idx_name = 0, idx = 0;

    U64 text_model_mem_size = 0;
    U64 vision_model_mem_size = 0;
    void *text_model_mem_buffer = NULL;
    void *vision_model_mem_buffer = NULL;

    ggml_context *temp_ctx = NULL;
    gguf_init_params params = {.no_alloc = false,
                               .ctx = &temp_ctx};

    gguf_context *gguf_ctx = gguf_init_from_file(CStrCast(model_path), params);
    if (!gguf_ctx)
    {
        res = {.success = 0,
               .domain = Domain_App,
               .code = AppError_NullPtr,
               .context = "gguf_ctx"};
        goto Cleanup;
    }

    n_tensors = gguf_get_n_tensors(gguf_ctx);
    n_kv = gguf_get_n_kv(gguf_ctx);
    ftype = get_u32(gguf_ctx, KEY_FTYPE);
    idx_desc = get_key_idx(gguf_ctx, KEY_DESCRIPTION);
    idx_name = gguf_find_key(gguf_ctx, KEY_NAME);
    if (idx_name != -1)
        mscbl_log_info("model name:   %s", gguf_get_val_str(gguf_ctx, idx_name));
    mscbl_log_info("description:  %s", gguf_get_val_str(gguf_ctx, idx_desc));
    mscbl_log_info("GGUF version: %d", gguf_get_version(gguf_ctx));
    mscbl_log_info("alignment:    %zu", gguf_get_alignment(gguf_ctx));
    mscbl_log_info("n_tensors:    %d", n_tensors);
    mscbl_log_info("n_kv:         %d", n_kv);
    mscbl_log_info("ftype:        %s", get_ftype(ftype));

    // kv
    for (S32 i = 0; i < n_kv; ++i)
    {
        const char *key = gguf_get_key(gguf_ctx, i);
        // mscbl_log_info("kv[%d]: key = %s", i, key);
    }

    // data
    for (S32 i = 0; i < n_tensors; ++i)
    {
        const char *name = gguf_get_tensor_name(gguf_ctx, i);
        const size_t offset = gguf_get_tensor_offset(gguf_ctx, i);

        // mscbl_log_info("tensor[%d]: name = %s, offset=%zu", i, name, offset);
    }

    // model size and capabilities
    idx = get_key_idx(gguf_ctx, KEY_HAS_TEXT_ENC);
    clip->has_text_encoder = gguf_get_val_bool(gguf_ctx, idx);

    idx = get_key_idx(gguf_ctx, KEY_HAS_VIS_ENC);
    clip->has_vision_encoder = gguf_get_val_bool(gguf_ctx, idx);

    idx = get_key_idx(gguf_ctx, KEY_USE_GELU);
    clip->use_gelu = gguf_get_val_bool(gguf_ctx, idx);

    mscbl_log_info("text_encoder:   %d", clip->has_text_encoder);
    mscbl_log_info("vision_encoder: %d", clip->has_vision_encoder);
    mscbl_log_info("model size:     %.2f MB", gguf_get_meta_size(gguf_ctx) / (float)MB(1));

    text_model_mem_size = 0;
    vision_model_mem_size = 0;
    for (S32 i = 0; i < n_tensors; ++i)
    {
        const char *name = gguf_get_tensor_name(gguf_ctx, i);

        // NOTE: Hacky-ish, but works fine for this model
        if (name[0] == 't')
            text_model_mem_size++;
        if (name[0] == 'v')
            vision_model_mem_size++;
    }
    text_model_mem_size *= ggml_tensor_overhead();
    vision_model_mem_size *= ggml_tensor_overhead();

    text_model_mem_buffer = arena_push(arena, text_model_mem_size, 0, GGML_MEM_ALIGN);
    clip->text_model.ctx_ggml = ggml_init({.mem_size = text_model_mem_size,
                                           .mem_buffer = text_model_mem_buffer,
                                           .no_alloc = true});
    if (!clip->text_model.ctx_ggml)
    {
        res = {.success = 0,
               .domain = Domain_App,
               .code = AppError_NullPtr,
               .context = "text_model.ggml_ctx"};
        goto Cleanup;
    }

    vision_model_mem_buffer = arena_push(arena, vision_model_mem_size, 0, GGML_MEM_ALIGN);
    clip->vision_model.ctx_ggml = ggml_init({.mem_size = vision_model_mem_size,
                                             .mem_buffer = vision_model_mem_buffer,
                                             .no_alloc = true});
    if (!clip->vision_model.ctx_ggml)
    {
        res = {.success = 0,
               .domain = Domain_App,
               .code = AppError_NullPtr,
               .context = "vision_model.ggml_ctx"};
        goto Cleanup;
    }

    for (S32 i = 0; i < n_tensors; ++i)
    {
        const char *name = gguf_get_tensor_name(gguf_ctx, i);
        ggml_tensor *src = ggml_get_tensor(temp_ctx, name);

        // NOTE: Hacky-ish, but works fine for this model
        if (name[0] == 't')
        {
            ggml_tensor *dst = ggml_dup_tensor(clip->text_model.ctx_ggml, src);
            ggml_set_name(dst, name);
        }
        if (name[0] == 'v')
        {
            ggml_tensor *dst = ggml_dup_tensor(clip->vision_model.ctx_ggml, src);
            ggml_set_name(dst, name);
        }
    }

    if (clip->has_text_encoder)
        clip_load_text_model(arena, clip, &clip->text_model, temp_ctx, gguf_ctx);

    if (clip->has_vision_encoder)
        clip_load_vision_model(arena, clip, &clip->vision_model, temp_ctx, gguf_ctx);

    mscbl_log_info("Used mem: %zu", ggml_used_mem(temp_ctx));

Cleanup:
    if (temp_ctx)
        ggml_free(temp_ctx);
    if (gguf_ctx)
        gguf_free(gguf_ctx);
    return res;
}

// TODO: This is definitely cursed
bool clip_tokenize(clip_ctx *ctx, String *input, clip_tokens *tokens)
{
    Assert(ctx->has_text_encoder, "This GGUF file seems to have no text encoder");

    std::vector<std::string> words;

    // first split the text into words
    {
        std::string pat = R"('s|'t|'re|'ve|'m|'ll|'d| ?[[:alpha:]]+| ?[[:digit:]]+| ?[^\s[:alpha:][:digit:]]+|\s+(?!\S)|\s+)";

        // Generate the subpattern from the special_tokens vector if it's not empty
        if (!ctx->vocab.special_tokens.empty())
        {
            std::string special_tokens_subpattern;
            for (const auto &token : ctx->vocab.special_tokens)
            {
                if (!special_tokens_subpattern.empty())
                {
                    special_tokens_subpattern += "|";
                }
                special_tokens_subpattern += token;
            }

            // Modify the regex pattern with the generated special tokens
            // subpattern
            pat = special_tokens_subpattern + "|" + pat;
        }

        std::regex re(pat);
        std::smatch m;

        std::string str = (char *)input->v;
        while (std::regex_search(str, m, re))
        {
            for (auto x : m)
            {
                words.push_back(x);
            }
            str = m.suffix();
        }
    }

    std::vector<clip_vocab::id> v_tokens;
    v_tokens.push_back(49406); // startoftext

    for (const auto &word : words)
    {
        // feel lucky? let's try if it's a full word
        std::string full_word = "";
        if (word.find(" ") == 0) // starts_with for C++11
        {
            full_word += word.substr(1);
        }
        else
        {
            full_word += word;
        }
        full_word += "</w>";
        auto wit = ctx->vocab.token_to_id.find(full_word);
        if (wit != ctx->vocab.token_to_id.end())
        {
            v_tokens.push_back(wit->second);
            continue;
        }

        for (S32 i = 0; i < word.size();)
        {
            for (S32 j = word.size() - 1; j >= i; j--)
            {
                auto cand = word.substr(i, j - i + 1);
                auto it = ctx->vocab.token_to_id.find(cand);
                if (it != ctx->vocab.token_to_id.end())
                { // word.substr(i, j-i+1) in vocab
                    v_tokens.push_back(it->second);
                    i = j + 1;
                    break;
                }
                else if (j == i)
                { // word.substr(i, 1) has no matching
                    fprintf(stderr, "%s: unknown token '%s'\n", __func__, word.substr(i, 1).data());
                    i++;
                }
            }
        }
    }

    v_tokens.push_back(49407); // endoftext

    tokens->size = v_tokens.size();

    tokens->data = new int[v_tokens.size()];
    std::copy(v_tokens.begin(), v_tokens.end(), tokens->data);

    return true;
}

// void clip_free(clip_ctx *ctx)
// {
//     ggml_free(ctx->ctx_ggml);
//     delete ctx;
// }

ggml_cgraph *build_text_encode_graph(clip_text_model *text_model, clip_ctx *clip, U64 token_size)
{
    clip_text_hparams hparams = text_model->hparams;

    F32 eps = hparams.eps;
    S32 n_vocab = hparams.n_vocab;
    S32 hidden_size = hparams.hidden_size;
    S32 num_positions = hparams.num_positions;
    S32 n_head = hparams.n_head;
    S32 d_head = hidden_size / n_head;
    S32 n_layer = hparams.n_layer;
    S32 n_intermediate = hparams.n_intermediate;
    S32 projection_dim = hparams.projection_dim;

    ggml_context *graph_ctx = text_model->graph_ctx;
    ggml_cgraph *graph = ggml_new_graph(graph_ctx);

    ggml_tensor *input_ids = ggml_new_tensor_1d(graph_ctx, GGML_TYPE_I32, token_size);
    ggml_set_name(input_ids, "input");
    ggml_set_input(input_ids);
    // memcpy(input_ids->data, tokens->data, N * ggml_element_size(input_ids));

    ggml_tensor *positions = ggml_new_tensor_1d(graph_ctx, GGML_TYPE_I32, token_size);
    ggml_set_name(positions, "positions");

    ggml_tensor *embeddings = ggml_get_rows(graph_ctx, text_model->token_embeddings, input_ids);

    embeddings = ggml_add(graph_ctx, ggml_get_rows(graph_ctx, text_model->position_embeddings, positions), embeddings);

    // loop over layers
    for (S32 il = 0; il < n_layer; il++)
    {
        ggml_tensor *cur = embeddings; // embeddings = residual, cur = hidden_states

        // layernorm1
        {
            cur = ggml_norm(graph_ctx, cur, eps);

            cur = ggml_add(graph_ctx, ggml_mul(graph_ctx, ggml_repeat(graph_ctx, text_model->layers[il].ln_1_w, cur), cur), ggml_repeat(graph_ctx, text_model->layers[il].ln_1_b, cur));
        }

        // self-attention
        {
            ggml_tensor *Q = ggml_add(graph_ctx, ggml_repeat(graph_ctx, text_model->layers[il].q_b, cur), ggml_mul_mat(graph_ctx, text_model->layers[il].q_w, cur));

            Q = ggml_scale_inplace(graph_ctx, Q, 1.0f / sqrt(float(d_head)));
            Q = ggml_reshape_4d(graph_ctx, Q, d_head, n_head, token_size, 1);
            Q = ggml_cont(graph_ctx, ggml_permute(graph_ctx, Q, 0, 2, 1, 3));
            Q = ggml_reshape_3d(graph_ctx, Q, d_head, token_size, n_head);

            ggml_tensor *K = ggml_add(graph_ctx, ggml_repeat(graph_ctx, text_model->layers[il].k_b, cur), ggml_mul_mat(graph_ctx, text_model->layers[il].k_w, cur));

            K = ggml_reshape_4d(graph_ctx, K, d_head, n_head, token_size, 1);
            K = ggml_cont(graph_ctx, ggml_permute(graph_ctx, K, 0, 2, 1, 3));
            K = ggml_reshape_3d(graph_ctx, K, d_head, token_size, n_head);

            ggml_tensor *V = ggml_add(graph_ctx, ggml_repeat(graph_ctx, text_model->layers[il].v_b, cur), ggml_mul_mat(graph_ctx, text_model->layers[il].v_w, cur));
            V = ggml_reshape_4d(graph_ctx, V, d_head, n_head, token_size, 1);
            V = ggml_cont(graph_ctx, ggml_permute(graph_ctx, V, 1, 2, 0, 3));
            V = ggml_reshape_3d(graph_ctx, V, token_size, d_head, n_head);

            ggml_tensor *KQ = ggml_mul_mat(graph_ctx, K, Q);
            KQ = ggml_diag_mask_inf_inplace(graph_ctx, KQ, 0); // causal masking
            KQ = ggml_soft_max_inplace(graph_ctx, KQ);

            ggml_tensor *KQV = ggml_mul_mat(graph_ctx, V, KQ);
            KQV = ggml_reshape_4d(graph_ctx, KQV, d_head, token_size, n_head, 1);
            KQV = ggml_cont(graph_ctx, ggml_permute(graph_ctx, KQV, 0, 2, 1, 3));

            cur = ggml_cpy(graph_ctx, KQV, ggml_new_tensor_2d(graph_ctx, GGML_TYPE_F32, hidden_size, token_size));
        }

        // attention output
        cur = ggml_add(graph_ctx, ggml_repeat(graph_ctx, text_model->layers[il].o_b, cur), ggml_mul_mat(graph_ctx, text_model->layers[il].o_w, cur));

        // re-add the layer input, e.g., residual
        cur = ggml_add(graph_ctx, cur, embeddings);

        embeddings = cur; // embeddings = residual, cur = hidden_states

        // layernorm2
        {
            cur = ggml_norm(graph_ctx, cur, eps);

            cur = ggml_add(graph_ctx, ggml_mul(graph_ctx, ggml_repeat(graph_ctx, text_model->layers[il].ln_2_w, cur), cur), ggml_repeat(graph_ctx, text_model->layers[il].ln_2_b, cur));
        }

        cur = ggml_mul_mat(graph_ctx, text_model->layers[il].ff_i_w, cur);
        cur = ggml_add(graph_ctx, ggml_repeat(graph_ctx, text_model->layers[il].ff_i_b, cur), cur);

        if (clip->use_gelu)
        {
            cur = ggml_gelu_inplace(graph_ctx, cur);
        }
        else
        {
            cur = ggml_gelu_quick_inplace(graph_ctx, cur);
        }

        cur = ggml_mul_mat(graph_ctx, text_model->layers[il].ff_o_w, cur);
        cur = ggml_add(graph_ctx, ggml_repeat(graph_ctx, text_model->layers[il].ff_o_b, cur), cur);

        // residual 2
        cur = ggml_add(graph_ctx, embeddings, cur);

        embeddings = cur;
    }

    // final -layer_norm
    {
        embeddings = ggml_norm(graph_ctx, embeddings, eps);

        embeddings = ggml_add(graph_ctx, ggml_mul(graph_ctx, ggml_repeat(graph_ctx, text_model->post_ln_w, embeddings), embeddings), ggml_repeat(graph_ctx, text_model->post_ln_b, embeddings));
    }

    // get the output of eot token, e.g., last index
    ggml_tensor *eot = ggml_new_tensor_1d(graph_ctx, GGML_TYPE_I32, 1);
    ggml_set_name(eot, "eot");
    embeddings = ggml_get_rows(graph_ctx, embeddings, eot);

    // text projection
    embeddings = ggml_mul_mat(graph_ctx, text_model->projection, embeddings);

    // TODO: Skipping normalization of output for now normalise output embeddings
    // normalize output embeddings
    // if (normalize)
    // {
    //     ggml_tensor *length = ggml_sqrt(graph_ctx, ggml_sum(graph_ctx, ggml_sqr(graph_ctx, embeddings)));
    //     assert(ggml_nbytes(length) == 0 && "Wrong function call here maybe");
    //     float lengthF = ggml_get_data_f32(length)[0];
    //     embeddings    = ggml_scale_inplace(graph_ctx, embeddings, 1.0f / lengthF);
    // }

    ggml_set_name(embeddings, "output");
    ggml_set_output(embeddings);

    // run the computation

    ggml_build_forward_expand(graph, embeddings);
    return graph;
}

Embedding clip_get_text_embedding(Arena *arena, clip_ctx *clip, clip_tokens *tokens)
{
    clip_text_model *text_model = &clip->text_model;
    clip_text_hparams hparams = text_model->hparams;
    S32 num_positions = hparams.num_positions;
    U64 token_size = tokens->size;

    ggml_cgraph *graph = build_text_encode_graph(text_model, clip, token_size);
    ggml_gallocr_t allocr = ggml_gallocr_new(ggml_backend_get_default_buffer_type(text_model->backend));
    ggml_gallocr_alloc_graph(allocr, graph);

    ggml_tensor *input = ggml_graph_get_tensor(graph, "input");
    ggml_backend_tensor_set(input, tokens->data, 0, ggml_nbytes(input));

    S32 eot_n = token_size - 1;
    ggml_tensor *eot = ggml_graph_get_tensor(graph, "eot");
    ggml_backend_tensor_set(eot, &eot_n, 0, ggml_nbytes(eot));

    S32 *pos_data = push_array(arena, num_positions, S32);
    for (S32 i = 0; i < token_size; i++)
        pos_data[i] = i;
    ggml_tensor *positions = ggml_graph_get_tensor(graph, "positions");
    ggml_backend_tensor_set(positions, pos_data, 0, ggml_nbytes(positions));

    perf_beg(compute);
    ggml_backend_graph_compute(text_model->backend, graph);
    perf_end(compute);

    ggml_tensor *output = ggml_graph_get_tensor(graph, "output");
    F32 *vector = push_array(arena, hparams.projection_dim, F32);
    MemoryCopy(vector, ggml_get_data(output), hparams.projection_dim * sizeof(F32));

    void *graph_mem = ggml_get_mem_buffer(text_model->graph_ctx);
    ggml_free(text_model->graph_ctx);
    ggml_gallocr_free(allocr);

    U64 mem_size = ggml_tensor_overhead() * GGML_DEFAULT_GRAPH_SIZE + ggml_graph_overhead();
    text_model->graph_ctx = ggml_init({.mem_size = mem_size,
                                       .mem_buffer = graph_mem,
                                       .no_alloc = true});

    return {
        .vector = vector,
        .size = hparams.projection_dim,
        .batch_size = 1};
}

ggml_cgraph *build_image_encode_graph(clip_vision_model *vision_model, clip_ctx *clip, S32 batch_size)
{
    clip_vision_hparams hparams = vision_model->hparams;

    F32 eps = hparams.eps;
    S32 image_size = hparams.image_size;
    S32 patch_size = hparams.patch_size;
    S32 num_patches = ((image_size / patch_size) * (image_size / patch_size));
    S32 num_positions = num_patches + 1;
    S32 hidden_size = hparams.hidden_size;
    S32 n_head = hparams.n_head;
    S32 d_head = hidden_size / n_head;
    S32 n_layer = hparams.n_layer;

    ggml_context *graph_ctx = vision_model->graph_ctx;
    ggml_cgraph *graph = ggml_new_graph(graph_ctx);

    ggml_tensor *input_raw = ggml_new_tensor_4d(graph_ctx, GGML_TYPE_F32, image_size, image_size, 3, batch_size);
    ggml_set_name(input_raw, "input");
    ggml_set_input(input_raw);

    ggml_tensor *input = ggml_conv_2d(graph_ctx, vision_model->patch_embeddings, input_raw, patch_size, patch_size, 0, 0, 1, 1);

    input = ggml_reshape_3d(graph_ctx, input, num_patches, hidden_size, batch_size);
    input = ggml_cont(graph_ctx, ggml_permute(graph_ctx, input, 1, 0, 2, 3));

    ggml_tensor *embeddings = ggml_new_tensor_3d(graph_ctx, GGML_TYPE_F32, hidden_size, num_positions, batch_size);
    ggml_set_name(embeddings, "embeddings");

    ggml_tensor *temp = ggml_new_tensor_3d(graph_ctx, GGML_TYPE_F32, hidden_size, 1, batch_size);

    embeddings = ggml_acc(graph_ctx, embeddings, ggml_repeat(graph_ctx, vision_model->class_embedding, temp), embeddings->nb[1],
                          embeddings->nb[2], embeddings->nb[3], 0);
    embeddings = ggml_acc(graph_ctx, embeddings, input, embeddings->nb[1], embeddings->nb[2], embeddings->nb[3], vision_model->class_embedding->nb[1]);

    ggml_tensor *positions = ggml_new_tensor_1d(graph_ctx, GGML_TYPE_I32, num_positions);
    ggml_set_name(positions, "positions");

    embeddings = ggml_add(graph_ctx, embeddings,
                          ggml_repeat(graph_ctx, ggml_get_rows(graph_ctx, vision_model->position_embeddings, positions), embeddings));

    // Pre-layernorm
    embeddings = ggml_norm(graph_ctx, embeddings, eps);
    embeddings = ggml_add(graph_ctx, ggml_mul(graph_ctx, ggml_repeat(graph_ctx, vision_model->pre_ln_w, embeddings), embeddings),
                          ggml_repeat(graph_ctx, vision_model->pre_ln_b, embeddings));

    // loop over layers
    for (S32 layer = 0; layer < n_layer; layer++)
    {
        ggml_tensor *cur = embeddings;

        // Layernorm 1
        cur = ggml_norm(graph_ctx, cur, eps);
        cur = ggml_add(graph_ctx, ggml_mul(graph_ctx, ggml_repeat(graph_ctx, vision_model->layers[layer].ln_1_w, cur), cur),
                       ggml_repeat(graph_ctx, vision_model->layers[layer].ln_1_b, cur));

        // self-attention
        ggml_tensor *Q = ggml_add(graph_ctx, ggml_repeat(graph_ctx, vision_model->layers[layer].q_b, cur),
                                  ggml_mul_mat(graph_ctx, vision_model->layers[layer].q_w, cur));
        Q = ggml_scale_inplace(graph_ctx, Q, 1 / sqrt((float)d_head));
        Q = ggml_reshape_4d(graph_ctx, Q, d_head, n_head, num_positions, batch_size);
        Q = ggml_cont(graph_ctx, ggml_permute(graph_ctx, Q, 0, 2, 1, 3));
        Q = ggml_reshape_3d(graph_ctx, Q, d_head, num_positions, n_head * batch_size);

        ggml_tensor *K = ggml_add(graph_ctx, ggml_repeat(graph_ctx, vision_model->layers[layer].k_b, cur),
                                  ggml_mul_mat(graph_ctx, vision_model->layers[layer].k_w, cur));
        K = ggml_reshape_4d(graph_ctx, K, d_head, n_head, num_positions, batch_size);
        K = ggml_cont(graph_ctx, ggml_permute(graph_ctx, K, 0, 2, 1, 3));
        K = ggml_reshape_3d(graph_ctx, K, d_head, num_positions, n_head * batch_size);

        ggml_tensor *V = ggml_add(graph_ctx, ggml_repeat(graph_ctx, vision_model->layers[layer].v_b, cur),
                                  ggml_mul_mat(graph_ctx, vision_model->layers[layer].v_w, cur));

        V = ggml_reshape_4d(graph_ctx, V, d_head, n_head, num_positions, batch_size);
        V = ggml_cont(graph_ctx, ggml_permute(graph_ctx, V, 1, 2, 0, 3));
        V = ggml_reshape_3d(graph_ctx, V, num_positions, d_head, n_head * batch_size);

        ggml_tensor *KQ = ggml_mul_mat(graph_ctx, K, Q);
        KQ = ggml_soft_max(graph_ctx, KQ);

        ggml_tensor *KQV = ggml_mul_mat(graph_ctx, V, KQ);
        KQV = ggml_reshape_4d(graph_ctx, KQV, d_head, num_positions, n_head, batch_size);
        KQV = ggml_cont(graph_ctx, ggml_permute(graph_ctx, KQV, 0, 2, 1, 3));

        cur = ggml_cpy(graph_ctx, KQV, ggml_new_tensor_3d(graph_ctx, GGML_TYPE_F32, hidden_size, num_positions, batch_size));

        // Attention output
        cur = ggml_add(graph_ctx, ggml_repeat(graph_ctx, vision_model->layers[layer].o_b, cur), ggml_mul_mat(graph_ctx, vision_model->layers[layer].o_w, cur));

        cur = ggml_add(graph_ctx, cur, embeddings);

        embeddings = cur;

        // Layernorm 2
        cur = ggml_norm(graph_ctx, cur, eps);
        cur = ggml_add(graph_ctx, ggml_mul(graph_ctx, ggml_repeat(graph_ctx, vision_model->layers[layer].ln_2_w, cur), cur),
                       ggml_repeat(graph_ctx, vision_model->layers[layer].ln_2_b, cur));

        cur = ggml_mul_mat(graph_ctx, vision_model->layers[layer].ff_i_w, cur);
        cur = ggml_add(graph_ctx, ggml_repeat(graph_ctx, vision_model->layers[layer].ff_i_b, cur), cur);

        if (clip->use_gelu)
        {
            cur = ggml_gelu_inplace(graph_ctx, cur);
        }
        else
        {
            cur = ggml_gelu_quick_inplace(graph_ctx, cur);
        }

        cur = ggml_mul_mat(graph_ctx, vision_model->layers[layer].ff_o_w, cur);
        cur = ggml_add(graph_ctx, ggml_repeat(graph_ctx, vision_model->layers[layer].ff_o_b, cur), cur);

        // Residual 2
        cur = ggml_add(graph_ctx, embeddings, cur);

        embeddings = cur;
    }

    ggml_tensor *cls = ggml_new_tensor_1d(graph_ctx, GGML_TYPE_I32, batch_size);
    ggml_set_name(cls, "cls");
    embeddings = ggml_get_rows(graph_ctx, ggml_reshape_2d(graph_ctx, embeddings, hidden_size, num_positions * batch_size), cls);

    // post-layernorm
    embeddings = ggml_norm(graph_ctx, embeddings, eps);
    embeddings = ggml_add(graph_ctx, ggml_mul(graph_ctx, ggml_repeat(graph_ctx, vision_model->post_ln_w, embeddings), embeddings),
                          ggml_repeat(graph_ctx, vision_model->post_ln_b, embeddings));

    // final visual projection
    embeddings = ggml_mul_mat(graph_ctx, vision_model->projection, embeddings);
    ggml_set_name(embeddings, "output");

    // TODO: Skipping normalization of output for now normalise output embeddings
    //   ggml_tensor *output = ggml_new_tensor_2d(graph_ctx, GGML_TYPE_F32,
    //                                            hparams.projection_dim,
    //                                            batch_size);

    //   for (S32 b = 0; b < batch_size; b++) {
    //     ggml_tensor *row =
    // ggml_get_rows(graph_ctx, embeddings, ggml_new_i32(graph_ctx, b));
    //     output =
    //         ggml_acc_inplace(graph_ctx, output, row, output->nb[1],
    //         output->nb[2],
    //                          output->nb[3], b * ggml_nbytes(row));
    //   }

    ggml_build_forward_expand(graph, embeddings);
    // ggml_graph_dump_dot(graph, NULL, ROOT_DIR "/graph.dot");
    return graph;
}

Embedding clip_get_image_embedding(Arena *arena, clip_ctx *clip, ggml_cgraph *graph, F32 *imageData, S32 batch_size)
{
    clip_vision_model *vision_model = &clip->vision_model;
    clip_vision_hparams hparams = vision_model->hparams;
    S32 image_size = hparams.image_size;
    S32 patch_size = hparams.patch_size;
    S32 num_patches = ((image_size / patch_size) * (image_size / patch_size));
    S32 num_positions = num_patches + 1;

    ggml_tensor *output = NULL;

    ArenaScoped(arena)
    {
        ggml_tensor *input = ggml_graph_get_tensor(graph, "input");
        U64 input_bytes = batch_size * 3 * image_size * image_size * sizeof(F32);
        ggml_backend_tensor_set(input, imageData, 0, input_bytes);

        ggml_tensor *embeddings = ggml_graph_get_tensor(graph, "embeddings");
        ggml_backend_tensor_memset(embeddings, 0, 0, ggml_nbytes(embeddings));
        // size_t embed_size		= ggml_nbytes(embeddings);
        // memset(ggml_get_data(embeddings), 0, embed_size * sizeof(F32)); // This single line is so so important. It cost me so much of my lifespan to find this bug
        // ggml_backend_tensor_set(embeddings, embedding, 0, embeddingsSize);

        ggml_tensor *positions = ggml_graph_get_tensor(graph, "positions");
        S32 *pos_data = push_array(arena, num_positions, S32);
        for (S32 i = 0; i < num_positions; i++)
            pos_data[i] = i;
        ggml_backend_tensor_set(positions, pos_data, 0, ggml_nbytes(positions));

        // Re-using positionsData array since it's just setting i * num_positions
        // where 'i' is numbers from 0->n
        ggml_tensor *cls = ggml_graph_get_tensor(graph, "cls");
        S32 *cls_data = push_array(arena, batch_size, S32);
        for (S32 i = 0; i < batch_size; i++)
            cls_data[i] = i * num_positions;
        ggml_backend_tensor_set(cls, cls_data, 0, batch_size * sizeof(S32));

        perf_beg(compute);
        ggml_backend_graph_compute(vision_model->backend, graph);
        perf_end(compute);

        output = ggml_graph_get_tensor(graph, "output");
    }

    return {
        .vector = (F32 *)ggml_get_data(output),
        .size = hparams.projection_dim,
        .batch_size = batch_size};
}

// float clip_similarity_score(const float *vec1, const float *vec2,
//                             const int vec_dim) {
//   float dot_product = 0.0;
//   for (int i = 0; i < vec_dim; i++) {
//     dot_product += vec1[i] * vec2[i];
//   }

//   return dot_product;
// }

// bool clip_compare_text_and_image(clip_ctx *ctx, const int n_threads,
//                                  const char *text, const clip_image_u8
//                                  *image, float *score) {
//   if (!(ctx->has_text_encoder && ctx->has_vision_encoder)) {
//     printf("clip_compare_text_and_image function can only be used with "
//            "two-tower models\n");
//     return false;
//   }

//   // prepare image and text vectors
//   const int projection_dim = ctx->vision_model.hparams.projection_dim;
//   float *img_vec = new float[projection_dim];
//   float *txt_vec = new float[projection_dim];

//   // tokenize and encode text
//   clip_tokens tokens;
//   if (!clip_tokenize(ctx, text, &tokens)) {
//     return false;
//   }

//   if (!clip_text_encode(ctx, n_threads, &tokens, txt_vec, true)) {
//     return false;
//   }

//   // preprocess and encode image
//   clip_image_f32 img_res;

//   if (!clip_image_preprocess(ctx, image, &img_res)) {
//     return false;
//   }

//   if (!clip_image_encode(ctx, n_threads, &img_res, img_vec, true)) {
//     return false;
//   }

//   // compute similarity
//   *score = clip_similarity_score(img_vec, txt_vec, projection_dim);

//   delete[] img_vec;
//   delete[] txt_vec;
//   return true;
// }

// typedef struct
// {
//     float score;
//     S32 index;
// } ScoreIndexPair;

// S32 compare_scores(const void *a, const void *b)
// {
//     const ScoreIndexPair *pair1 = (const ScoreIndexPair *)a;
//     const ScoreIndexPair *pair2 = (const ScoreIndexPair *)b;
//
//     if (pair1->score < pair2->score)
//     {
//         return 1;
//     }
//     else if (pair1->score > pair2->score)
//     {
//         return -1;
//     }
//     else
//     {
//         return 0;
//     }
// }

// bool softmax_with_sorting(float *arr, const int length, float *sorted_scores, int *indices)
// {
//     ScoreIndexPair *score_index_pairs = (ScoreIndexPair *)malloc(length * sizeof(ScoreIndexPair));
//     if (!score_index_pairs)
//     {
//         return false;
//     }
//
//     // Calculate softmax probabilities
//
//     double sum = 0.0;
//     for (int i = 0; i < length; i++)
//     {
//         arr[i] = exp(arr[i]) + 1e-9;
//         sum += arr[i];
//     }
//
//     for (int i = 0; i < length; i++)
//     {
//         arr[i] /= sum;
//         score_index_pairs[i].score = arr[i];
//         score_index_pairs[i].index = i;
//     }
//
//     // Sort scores in descending order
//     qsort(score_index_pairs, length, sizeof(ScoreIndexPair), compare_scores);
//
//     // Copy sorted scores and indices to the respective arrays
//     for (int i = 0; i < length; i++)
//     {
//         sorted_scores[i] = score_index_pairs[i].score;
//         indices[i] = score_index_pairs[i].index;
//     }
//
//     free(score_index_pairs);
//     return true;
// }

// bool clip_zero_shot_label_image(struct clip_ctx *ctx, const int n_threads,
//                                 const struct clip_image_u8 *input_img,
//                                 const char **labels, const size_t n_labels,
//                                 float *scores, int *indices) {
//   if (!(ctx->has_text_encoder && ctx->has_vision_encoder)) {
//     printf("clip_zero_shot_label_image function can only be used with "
//            "two-tower models\n");
//     return false;
//   }

//   // load the image
//   clip_image_f32 img_res;

//   const int vec_dim = clip_get_vision_hparams(ctx)->projection_dim;

//   clip_image_preprocess(ctx, input_img, &img_res);

//   float *img_vec = new float[vec_dim];
//   if (!clip_image_encode(ctx, n_threads, &img_res, img_vec, false)) {
//     return false;
//   }

//   // encode texts and compute similarities
//   float *txt_vec = new float[vec_dim];
//   float *similarities = new float[n_labels];

//   for (int i = 0; i < n_labels; i++) {
//     const auto &text = labels[i];
//     clip_tokens tokens;
//     clip_tokenize(ctx, text, &tokens);
//     clip_text_encode(ctx, n_threads, &tokens, txt_vec, false);
//     similarities[i] = clip_similarity_score(img_vec, txt_vec, vec_dim);
//   }

//   // apply softmax and sort scores
//   softmax_with_sorting(similarities, n_labels, scores, indices);

//   delete[] img_vec;
//   delete[] txt_vec;
//   delete[] similarities;

//   return true;
// }

// bool clip_model_quantize(const char *fname_inp, const char *fname_out, const int itype)
// {
// 	ggml_type type = GGML_TYPE_Q4_1;

// 	switch (itype)
// 	{
// 	case 2:
// 		type = GGML_TYPE_Q4_0;
// 		break;
// 	case 3:
// 		type = GGML_TYPE_Q4_1;
// 		break;
// 	case 6:
// 		type = GGML_TYPE_Q5_0;
// 		break;
// 	case 7:
// 		type = GGML_TYPE_Q5_1;
// 		break;
// 	case 8:
// 		type = GGML_TYPE_Q8_0;
// 		break;
// 	default:
// 		fprintf(stderr, "%s: invalid quantization type %d\n", __func__, itype);
// 		return false;
// 	};

// 	clip_ctx ctx_clip	 = clip_model_load(fname_inp);
// 	const auto &ctx_src	 = ctx_clip.ctx_gguf;
// 	const auto &ctx_data = ctx_clip.ctx_ggml;

// 	auto ctx_out = gguf_init_empty();
// 	gguf_set_kv(ctx_out, ctx_src);
// 	gguf_set_val_u32(ctx_out, "general.quantization_version", GGML_QNT_VERSION);
// 	gguf_set_val_u32(ctx_out, "general.file_type", itype);

// 	FILE *fout = fopen(fname_out, "wb");

// 	const int n_tensors = gguf_get_n_tensors(ctx_src);

// 	for (int i = 0; i < n_tensors; ++i)
// 	{
// 		const char *name = gguf_get_tensor_name(ctx_src, i);
// 		ggml_tensor *cur = ggml_get_tensor(ctx_data, name);
// 		gguf_add_tensor(ctx_out, cur);
// 	}

// 	const size_t meta_size = gguf_get_meta_size(ctx_out);
// 	for (size_t i = 0; i < meta_size; ++i)
// 	{
// 		fputc(0, fout);
// 	}

// 	// regexes of tensor names to be quantized
// 	const std::vector<std::string> k_names = {
// 		".*weight",
// 	};

// 	std::vector<uint8_t> read_data(512);
// 	std::vector<uint8_t> work(512);
// 	std::vector<float> conv_buf(512);
// 	std::vector<int64_t> hist_all(1 << 4, 0);
// 	size_t total_size_org = 0;
// 	size_t total_size_new = 0;

// 	for (int i = 0; i < n_tensors; ++i)
// 	{
// 		const std::string name = gguf_get_tensor_name(ctx_src, i);
// 		ggml_tensor *cur	   = ggml_get_tensor(ctx_data, name.c_str());

// 		enum ggml_type new_type;
// 		void *new_data;
// 		size_t new_size;

// 		bool quantize = false;
// 		for (const auto &s : k_names)
// 		{
// 			if (std::regex_match(name, std::regex(s)))
// 			{
// 				quantize = true;
// 				break;
// 			}
// 		}

// 		// quantize only 2D tensors
// 		quantize &= (ggml_n_dims(cur) == 2);

// 		if (quantize)
// 		{
// 			new_type			= type;
// 			const size_t n_elms = ggml_nelements(cur);
// 			float *f32_data;

// 			switch (cur->type)
// 			{
// 			case GGML_TYPE_F32:
// 				f32_data = (float *)cur->data;
// 				break;
// 			case GGML_TYPE_F16:
// 				if (conv_buf.size() < n_elms)
// 				{
// 					conv_buf.resize(n_elms);
// 				}
// 				for (int j = 0; j < n_elms; ++j)
// 				{
// 					conv_buf[j] = ggml_fp16_to_fp32(((ggml_fp16_t *)cur->data)[j]);
// 				}
// 				f32_data = (float *)conv_buf.data();
// 				break;
// 			default:
// 				printf("Please use an input file in f32 or f16\n");
// 				return false;
// 			}

// 			if (work.size() < n_elms * 4)
// 			{
// 				work.resize(n_elms * 4);
// 			}
// 			new_data = work.data();

// 			std::vector<int64_t> hist_cur(1 << 4, 0);

// 			// switch (new_type) {
// 			// case GGML_TYPE_Q4_0: {
// 			//   new_size = ggml_quantize_q4_0(f32_data, new_data, n_elms,
// 			//   cur->ne[0],
// 			//                                 hist_cur.data());
// 			// } break;
// 			// case GGML_TYPE_Q4_1: {
// 			//   new_size = ggml_quantize_q4_1(f32_data, new_data, n_elms,
// 			//   cur->ne[0],
// 			//                                 hist_cur.data());
// 			// } break;
// 			// case GGML_TYPE_Q5_0: {
// 			//   new_size = ggml_quantize_q5_0(f32_data, new_data, n_elms,
// 			//   cur->ne[0],
// 			//                                 hist_cur.data());
// 			// } break;
// 			// case GGML_TYPE_Q5_1: {
// 			//   new_size = ggml_quantize_q5_1(f32_data, new_data, n_elms,
// 			//   cur->ne[0],
// 			//                                 hist_cur.data());
// 			// } break;
// 			// case GGML_TYPE_Q8_0: {
// 			//   new_size = ggml_quantize_q8_0(f32_data, new_data, n_elms,
// 			//   cur->ne[0],
// 			//                                 hist_cur.data());
// 			// } break;
// 			// default: {
// 			//   fprintf(stderr, "%s: unsupported quantization type %d\n",
// 			//   __func__,
// 			//           new_type);
// 			//   return false;
// 			// }
// 			// }

// 			for (int j = 0; j < hist_cur.size(); ++j)
// 			{
// 				hist_all[j] += hist_cur[j];
// 			}
// 		}
// 		else
// 		{
// 			new_type = cur->type;
// 			new_data = cur->data;
// 			new_size = ggml_nbytes(cur);
// 		}
// 		const size_t orig_size = ggml_nbytes(cur);
// 		total_size_org += orig_size;
// 		total_size_new += new_size;
// 		gguf_set_tensor_type(ctx_out, name.c_str(), new_type);
// 		gguf_set_tensor_data(ctx_out, name.c_str(), new_data);
// 		fwrite(new_data, 1, new_size, fout);
// 		size_t pad = GGML_PAD(new_size, gguf_get_alignment(ctx_out)) - new_size;
// 		for (int j = 0; j < pad; ++j)
// 		{
// 			fputc(0, fout);
// 		}

// 		printf("%s: n_dims = %d | quantize=%d | size = %f MB -> %f MB\n", name.c_str(), ggml_n_dims(cur), quantize,
// 			   orig_size / 1024.0 / 1024.0, new_size / 1024.0 / 1024.0);
// 	}

// 	// go back to beginning of file and write the updated metadata
// 	fseek(fout, 0, SEEK_SET);
// 	std::vector<uint8_t> meta(meta_size);
// 	gguf_get_meta_data(ctx_out, meta.data());
// 	fwrite(meta.data(), 1, meta_size, fout);

// 	fclose(fout);

// 	clip_free(ctx_clip);
// 	gguf_free(ctx_out);

// 	{
// 		printf("%s: original size  = %8.2f MB\n", __func__, total_size_org / 1024.0 / 1024.0);
// 		printf("%s: quantized size  = %8.2f MB\n", __func__, total_size_new / 1024.0 / 1024.0);

// 		int64_t sum_all = 0;
// 		for (size_t i = 0; i < hist_all.size(); ++i)
// 		{
// 			sum_all += hist_all[i];
// 		}

// 		printf("%s: hist: ", __func__);
// 		for (size_t i = 0; i < hist_all.size(); ++i)
// 		{
// 			printf("%5.3f ", hist_all[i] / (float)sum_all);
// 		}
// 		printf("\n");
// 	}

// 	return true;
// }
