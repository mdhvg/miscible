#ifndef CLIP_H
#define CLIP_H

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "ggml.h"

struct clip_ctx;

#if defined(_WIN32)

#define NOMINMAX
#include <windows.h>

typedef HANDLE pthread_t;
typedef DWORD thread_ret_t;

#endif

#ifdef __cplusplus
extern "C"
{
#endif

	struct clip_text_hparams
	{
		int32_t n_vocab;
		int32_t num_positions;
		int32_t hidden_size;
		int32_t n_intermediate;
		int32_t projection_dim;
		int32_t n_head;
		int32_t n_layer;
		float eps;
	};

	struct clip_vision_hparams
	{
		int32_t image_size;
		int32_t patch_size;
		int32_t hidden_size;
		int32_t n_intermediate;
		int32_t projection_dim;
		int32_t n_head;
		int32_t n_layer;
		float eps;
	};

	typedef int32_t clip_vocab_id;
	struct clip_tokens
	{
		clip_vocab_id *data;
		size_t size;
	};

	struct clip_ctx *clip_model_load(const char *fname, const int verbosity);

	void clip_free(struct clip_ctx *ctx);

	struct clip_text_hparams *clip_get_text_hparams(struct clip_ctx *ctx);
	struct clip_vision_hparams *clip_get_vision_hparams(struct clip_ctx *ctx);

	// TODO: Assuming this works **as intended** for now. Need the check later
	bool clip_tokenize(struct clip_ctx *ctx, std::string &text, struct clip_tokens *tokens);

	bool clip_text_encode(struct clip_ctx *ctx, const int n_threads, const struct clip_tokens *tokens, float *vec,
						  const bool normalize);

	void clip_get_text_embedding(clip_ctx *clip, ggml_context *text_ctx, ggml_cgraph *text_graph, float *output);

	struct ggml_cgraph *build_text_encode_graph(ggml_context *text_ctx, clip_ctx *clip, clip_tokens *tokens);

	float *clip_get_image_embedding(struct clip_ctx *clip, ggml_context *vision_ctx, ggml_cgraph *vision_graph,
									float *imageData, int batch_size);

	struct ggml_cgraph *build_image_encode_graph(ggml_context *graph_ctx, clip_ctx *clip, int batch_size);

	// bool image_normalize(const clip_image_u8 *img, clip_image_f32 *res);

	// bool clip_compare_text_and_image(struct clip_ctx *ctx, const int n_threads,
	//                                  const char *text,
	//                                  const struct clip_image_u8 *image,
	//                                  float *score);
	// float clip_similarity_score(const float *vec1, const float *vec2,
	//                             const int vec_dim);
	bool softmax_with_sorting(float *arr, const int length, float *sorted_scores, int *indices);
	// bool clip_zero_shot_label_image(struct clip_ctx *ctx, const int n_threads,
	//                                 const struct clip_image_u8 *input_img,
	//                                 const char **labels, const size_t n_labels,
	//                                 float *scores, int *indices);

	bool clip_model_quantize(const char *fname_inp, const char *fname_out, const int itype);

#ifdef __cplusplus
}
#endif

#endif // CLIP_H
