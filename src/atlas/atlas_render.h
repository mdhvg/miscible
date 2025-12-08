#pragma once

#include "base/threadpool.h"

#define ATLAS_CAPACITY 100
#define ATLAS_SIZE	   2560
#define THUMB_PER_SIDE 10
#define THUMB_SIZE	   256

void atlas_init(void *unused);
THREAD_FUNC(load_thumbnails);