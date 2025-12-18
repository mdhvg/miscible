#include <time.h>

#include "arena.h"
#include "os/os_inc.h"
#include "base/threadpool.h"

static U64 start_t = 0;

void threadpool_run_tasks(ThreadPool *pool, Worker *worker)
{
	for (;;)
	{
		S64 task_left = ins_atomic_u64_dec_eval(&pool->task_left);

		if (task_left < 0)
		{
			break;
		}

		Arena *arena = pool->worker_arena && pool->worker_arena->v
						   ? pool->worker_arena->v[worker->id]
						   : NULL;
		pool->task_func(arena, task_left, worker->id, pool->task_data);

		U64 task_done = ins_atomic_u64_inc_eval(&pool->task_done);
		if (task_done == pool->task_count)
		{
			printf("Elapsed time: %.6f\n", ((double)(clock() - start_t)) / CLOCKS_PER_SEC);
			pool->available = 1;
		}
	}
}

OS_THREAD_ROUTINE(threadpool_worker)
{
	Worker *worker	 = (Worker *)data;
	ThreadPool *pool = worker->pool;
	for (; pool->active;)
	{
		if (os_semaphore_take(pool->task_semaphore, U64_MAX))
		{
			threadpool_run_tasks(pool, worker);
		}
	}
	return 0;
}

ThreadPool *threadpool_init(Arena *arena, U32 worker_count)
{
	Assert(worker_count > 0);

	Semaphore task_semaphore = {0};

	task_semaphore = os_semaphore_alloc(0, OS_SEMAPHORE_MAX); // TODO: Check if linux and windows have this as same value then just make 1 constant in "base/base_core.h"

	ThreadPool *pool = push_struct(arena, ThreadPool);
	*pool			 = {0};

	pool->task_semaphore = task_semaphore;
	pool->active		 = 1;
	pool->worker_count	 = worker_count;
	pool->worker_array	 = push_array(arena, worker_count, Worker);
	pool->available		 = 1;

	for (U64 i = 0; i < worker_count; i++)
	{
		Worker *worker = &pool->worker_array[i];
		worker->id	   = i;
		worker->pool   = pool;
	}

	for (U64 i = 1; i < worker_count; i += 1)
	{
		Worker *worker = &pool->worker_array[i];
		worker->handle = os_thread_launch(threadpool_worker, worker);
	}
	return pool;
}

void threadpool_free(ThreadPool *pool)
{
	pool->active = 0;
	for (U64 i = 0; i < pool->worker_count; ++i)
	{
		os_semaphore_drop(pool->task_semaphore);
	}
	for (U64 i = 1; i < pool->worker_count; i += 1)
	{
		os_thread_detach(pool->worker_array[i].handle);
	}
	os_semaphore_release(pool->task_semaphore);
	MemoryZeroStruct(pool);
}

B8 parallel_for(ThreadPool *pool, U64 task_count, thread_func *func, void *data, ArenaArray *worker_arena)
{
	start_t = clock();
	if (task_count <= 0) { return 0; }
	if (ins_atomic_u64_eval(&pool->task_done) != pool->task_count) { return 0; }

	pool->task_func	   = func;
	pool->task_data	   = data;
	pool->task_count   = task_count;
	pool->task_done	   = 0;
	pool->available	   = 0;
	pool->worker_arena = worker_arena;
	ins_atomic_u64_eval_assign(&pool->task_left, task_count);

	U64 drop_count = MIN(task_count, pool->worker_count);

	for (U64 i = 0; i < drop_count; i++)
	{
		os_semaphore_drop(pool->task_semaphore);
	}
	return 1;
}

// void threadpool_init(int n)
// {
// 	for (int i = 0; i < n; i++)
// 	{
// 		pool.workers.emplace_back([] {
// 			while (true)
// 			{
// 				Job job;
// 				{
// 					std::unique_lock<std::mutex> lock(pool.m);
// 					pool.cv.wait(lock, [] { return pool.closed || !pool.jobs.empty(); });
// 					if (pool.closed && pool.jobs.empty())
// 						return;
// 					job = pool.jobs.front();
// 					pool.jobs.pop();
// 				}
// 				job.func();
// 				if (job.block)
// 					job.block->unlock();
// 			}
// 		});
// 	}
// }

// void threadpool_enqueue(std::function<void()> job, std::mutex *block)
// {
// 	{
// 		std::lock_guard<std::mutex> lock(pool.m);
// 		if (pool.closed)
// 			return;
// 		pool.jobs.push({std::move(job), block});
// 	}
// 	pool.cv.notify_one();
// }

// void threadpool_enqueue(thread_func func, void *data, std::mutex *block)
// {
// 	{
// 		std::lock_guard<std::mutex> lock(pool.m);
// 		if (pool.closed)
// 			return;
// 		pool.jobs.push({[func, data]() { func(data); }, block});
// 	}
// 	pool.cv.notify_one();
// }

// void threadpool_destroy()
// {
// 	{
// 		std::lock_guard<std::mutex> lock(pool.m);
// 		pool.closed = true;
// 	}
// 	pool.cv.notify_all();
// 	for (auto &w : pool.workers)
// 		w.join();
// }
