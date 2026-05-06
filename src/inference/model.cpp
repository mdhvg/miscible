#include "base/log.h"
#include "db/db_helpers.h"
#include "inference/clip.h"
#include "inference/model.h"

Arena *model_arena = NULL;
CLIPModel model = {0};

#define MODEL_PATH "./CLIP-ViT-B-32-laion2B-s34B-b79K.gguf"

DBStmtCbk(print_dist)
{
    mscbl_log_dbg("id: %zu, path: %s, distance: %.6f", sqlite3_column_int64(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_double(stmt, 2));
}

Embedding embed_text(String text)
{
    if (!text.size)
        return {0};

    if (!model_arena)
        arena_alloc(MB(100), model_arena);
    if (!model.clip)
    {
        model.clip = push_struct(model_arena, clip_ctx);
        clip_model_load(model_arena, model.clip, MODEL_PATH);
    }

    clip_tokens tokens;
    clip_tokenize(model.clip, &text, &tokens);

    F32 *embedding = clip_get_text_embedding(model_arena, model.clip, &tokens, 0);

    // sqlite3_stmt *stmt = db_prepare("SELECT id, path, distance_cosine_f32(embedding, ?) AS distance FROM Images WHERE embedding IS NOT NULL ORDER BY distance ASC LIMIT 10;");
    // sqlite3_bind_blob(stmt, 1, embedding, 512 * sizeof(F32), SQLITE_STATIC);
    // mscbl_log_dbg("Returned %zu rows", db_run_stmt(stmt, 1, print_dist));
    return {embedding, clip_get_text_hparams(model.clip)->projection_dim};
}
