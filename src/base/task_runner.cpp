#include "base/task_runner.h"
#include "base/queue.h"
#include "inference/model.h"
#include "ui/ui_core.h"
#include "os/os_inc.h"
#include "atlas/atlas_render.h"

void task_runner_try(ThreadPool *pool)
{
	if (runner.active_task != TASK_NONE && os_info.pool->task_count == os_info.pool->task_done)
	{
		switch (runner.active_task)
		{
		case TASK_LOAD_THUMBNAILS:
			break;
		case TASK_DRAW_THUMBNAILS:
			atlas_after_draw();
			break;
		case TASK_LOAD_ATLAS:
			ui_after_load();
			break;
		case TASK_EMBED_IMAGES:
			model_after_create_embedding();
			break;
		default:
			break;
		}
		bufferqueue_pop(runner.pending_tasks);
		runner.active_task = TASK_NONE;
	}

	if (!bufferqueue_empty(runner.pending_tasks) && runner.active_task == TASK_NONE)
	{
		runner.active_task = *bufferqueue_front(runner.pending_tasks);
		switch (runner.active_task)
		{
		case TASK_WALK_DIRS: {
			walk_directories(NULL); // Sequential task, so clear immediately after running
			bufferqueue_pop(runner.pending_tasks);
			runner.active_task = TASK_NONE;
			break;
		}
		case TASK_LOAD_THUMBNAILS:
			atlas_load_batch();
			break;
		case TASK_DRAW_THUMBNAILS:
			atlas_draw_one();
			break;
		case TASK_LOAD_ATLAS:
			ui_count_atlas();
			break;
		case TASK_EMBED_IMAGES:
			model_create_embeddings();
			break;
		case TASK_NONE:
		default:
			break;
		};
	}
}