#include "Inference/index.h"

internal index_dense_t usearch_index;

#define INDEX_PATH ROOT_DIR "/index.usearch"

void init_index(unsigned int dimension)
{
	if (fs::exists(INDEX_PATH))
	{
		usearch_index.load(INDEX_PATH);
	}
	else
	{
		usearch_index = index_dense_t::make(metric_punned_t(dimension, metric_kind_t::cos_k));
	}
}

void add_embedding(unsigned int id, float *vector)
{
	// usearch_index.add()
}

void close_index()
{
	printf("Saving index at: %s\n", INDEX_PATH);
	usearch_index.save(INDEX_PATH);
}