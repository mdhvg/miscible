#include <queue>

#include "Inference/model.h"

float *make_planar(float *data, int image_size, int batch_size);

float *make_batch(std::queue<ImageEmbedding> *images, int batch_size, int image_size);