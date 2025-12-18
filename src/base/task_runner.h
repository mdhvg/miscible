#pragma once

#include "base/base_core.h"
#include "base/queue.h"
#include "base/threadpool.h"

enum Task
{
	TASK_NONE = 0,
	TASK_WALK_DIRS,
	TASK_LOAD_THUMBNAILS,
	TASK_DRAW_THUMBNAILS,
	TASK_LOAD_ATLAS,
	TASK_EMBED_IMAGES,
};

struct TaskRunner
{
	BUFFER_QUEUE(pending_tasks, Task, 100);
	Task active_task;
};

TaskRunner runner = {0};

void task_runner_try(ThreadPool *pool);

#define push_task(t) bufferqueue_push(runner.pending_tasks, t)