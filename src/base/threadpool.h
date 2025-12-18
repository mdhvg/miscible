#pragma once

#include "base/arena.h"
#include "base/base_core.h"
#include "base/threads.h"

#define THREAD_FUNC(name) void name(Arena *arena, U64 n, U64 id, void *data)
typedef THREAD_FUNC(thread_func);

struct Worker
{
	U64 id;
	struct ThreadPool *pool;
	Thread handle;
};

struct ThreadPool
{
	B16 active;
	U64 task_count;
	U64 task_done;
	S64 task_left;
	B8 available;

	U32 worker_count;
	Worker *worker_array;
	ArenaArray *worker_arena;

	Semaphore task_semaphore;

	thread_func *task_func;
	void *task_data;
};

ThreadPool *threadpool_init(Arena *arena, U32 worker_count);
void threadpool_free(ThreadPool *pool);
B8 parallel_for(ThreadPool *pool, U64 task_count, thread_func *func, void *data, ArenaArray *worker_arena);

// void threadpool_init(int n = 4);
// void threadpool_enqueue(std::function<void()> job, std::mutex *block = NULL);
// void threadpool_enqueue(thread_func func, void *data, std::mutex *block = NULL);
// void threadpool_destroy();