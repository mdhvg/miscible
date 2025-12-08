#include "Inference/model.h"

#include <cstddef>
#include <queue>
#include <string>

#include "Inference/clip.h"
#include "Inference/preprocess.h"
#include "db/db_helpers.h"
#include "ggml.h"

internal clip_ctx *clip = NULL;

internal ggml_context *vision_ctx = NULL;
internal ggml_cgraph *vision_graph = NULL;
internal clip_vision_hparams *vision_hparams = NULL;

internal ggml_context *text_ctx = NULL;
internal ggml_cgraph *text_graph = NULL;

clip_ctx *clip_init()
{
	clip = clip_model_load(ROOT_DIR "/CLIP-ViT-B-32-laion2B-s34B-b79K_ggml-model-f16.gguf", 10);
	vision_hparams = clip_get_vision_hparams(clip);
	return clip;
}

std::queue<ImageEmbedding> find_pending_embeddings()
{
	std::queue<ImageEmbedding> image_id_path;
	db_run(
		"SELECT id, path FROM Images WHERE embedding IS NULL;",
		[](void *data, int, char **argv, char **) {
			std::queue<ImageEmbedding> *image_id_path = (std::queue<ImageEmbedding> *)data;
			image_id_path->push({(unsigned int)std::stoul(argv[0]), argv[1]});
			return 0;
		},
		&image_id_path);
	return image_id_path;
}

void clip_process_images(std::queue<ImageEmbedding> *image_id_path)
{
	// while (!image_id_path->empty()) {
	//   int batch_size = MIN(image_id_path->size(), 4);
	//   float* batch = make_batch(image_id_path, batch_size, 224);

	//   const size_t graph_size = ggml_tensor_overhead() * GGML_DEFAULT_GRAPH_SIZE +
	//                             ggml_graph_overhead();
	//   ggml_init_params graph_params = {graph_size, nullptr, true};
	//   vision_ctx = ggml_init(graph_params);
	//   vision_graph = build_image_encode_graph(vision_ctx, clip, batch_size);
	//   float* embeddings = clip_get_image_embedding(clip, vision_ctx, vision_graph,
	//                                                batch, batch_size);
	//   SPDLOG_INFO("Made embeddings!");

	//   delete[] batch;
	//   delete[] embeddings;
	// }
}

clip_ctx *clip_get()
{
	return clip;
}
