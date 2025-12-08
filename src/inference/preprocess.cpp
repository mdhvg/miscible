#include "Inference/preprocess.h"

#include "Inference/model.h"
#include "base/core.h"
#include "stb_image.h"
#include "stb_image_resize2.h"

float *make_planar(float *data, int image_size, int batch_size)
{
	float *planar_data = new float[image_size * image_size * 3 * batch_size];
	int num_pixels = image_size * image_size;

	for (int i = 0; i < batch_size; i++)
	{
		float *r_plane = planar_data + num_pixels * (3 * i + 0);
		float *g_plane = planar_data + num_pixels * (3 * i + 1);
		float *b_plane = planar_data + num_pixels * (3 * i + 2);

		for (int pixel = 0; pixel < num_pixels; pixel++)
		{
			r_plane[pixel] = data[pixel * 3 + 0 + i * 3 * num_pixels];
			g_plane[pixel] = data[pixel * 3 + 1 + i * 3 * num_pixels];
			b_plane[pixel] = data[pixel * 3 + 2 + i * 3 * num_pixels];
		}
	}

	delete[] data;
	return planar_data;
}

float *make_batch(std::queue<ImageEmbedding> *images, int batch_size, int image_size)
{
	float *resized_data = new float[image_size * image_size * 3 * batch_size];

	float *_data;
	int _w, _h, _c;
	for (int i = 0; i < batch_size; i++)
	{
		_data = stbi_loadf((const char *)images->front().path.c_str(), &_w, &_h, NULL, 3);
		stbir_resize_float_linear(_data, _w, _h, 0, resized_data + (i * image_size * image_size * 3), image_size,
								  image_size, 0, STBIR_RGB);
		images->pop();
	}

	return make_planar(resized_data, image_size, batch_size);
}