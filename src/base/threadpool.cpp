#include "base/threadpool.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "base/core.h"

struct Job
{
	std::function<void()> func;
	std::mutex *block;
};

struct Threadpool
{
	std::queue<Job> jobs;
	std::condition_variable cv;
	std::mutex m;
	std::vector<std::thread> workers;
	bool closed = false;
};

internal Threadpool pool;

void threadpool_init(int n)
{
	for (int i = 0; i < n; i++)
	{
		pool.workers.emplace_back([] {
			while (true)
			{
				Job job;
				{
					std::unique_lock<std::mutex> lock(pool.m);
					pool.cv.wait(lock, [] { return pool.closed || !pool.jobs.empty(); });
					if (pool.closed && pool.jobs.empty())
						return;
					job = pool.jobs.front();
					pool.jobs.pop();
				}
				job.func();
				if (job.block)
					job.block->unlock();
			}
		});
	}
}

void threadpool_enqueue(std::function<void()> job, std::mutex *block)
{
	{
		std::lock_guard<std::mutex> lock(pool.m);
		if (pool.closed)
			return;
		pool.jobs.push({std::move(job), block});
	}
	pool.cv.notify_one();
}

void threadpool_enqueue(thread_func func, void *data, std::mutex *block)
{
	{
		std::lock_guard<std::mutex> lock(pool.m);
		if (pool.closed)
			return;
		pool.jobs.push({[func, data]() { func(data); }, block});
	}
	pool.cv.notify_one();
}

void threadpool_destroy()
{
	{
		std::lock_guard<std::mutex> lock(pool.m);
		pool.closed = true;
	}
	pool.cv.notify_all();
	for (auto &w : pool.workers)
		w.join();
}
