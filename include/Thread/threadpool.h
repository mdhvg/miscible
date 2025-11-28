#pragma once
#include <spdlog/spdlog.h>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

class ThreadPool
{
public:
	ThreadPool(size_t n)
			: nThreads(n), stop(false)
	{
		for (int i = 0; i < nThreads; ++i)
		{
			workers.emplace_back([this] {
				while (true)
				{
					std::unique_lock<std::mutex> lk(queueMutex);
					cv.wait(lk, [this] { return stop || !tasks.empty(); });
					if (stop)
						return;
					auto task = tasks.front();
					tasks.pop();
					lk.unlock();
					task();
				}
			});
		}
	}

	template <class F>
	void enqueue(F &&task, const std::string &name = "")
	{
		{
			std::lock_guard<std::mutex> lk(queueMutex);
			if (stop)
				return;
			if (!name.empty())
			{
				tasks.emplace([task = std::forward<F>(task), name]() mutable {
					auto start = std::chrono::high_resolution_clock::now();
					task();
					auto end = std::chrono::high_resolution_clock::now();
					std::chrono::duration<double> duration = end - start;
					SPDLOG_INFO("{} executed in: {:.6f} seconds", name, duration.count());
				});
			}
			else
			{
				tasks.emplace(std::forward<F>(task));
			}
		}
		cv.notify_one();
	}

	~ThreadPool()
	{
		std::unique_lock<std::mutex> lk(queueMutex);
		stop = true;
		lk.unlock();
		cv.notify_all();
		for (std::thread &worker : workers)
		{
			worker.join();
		}
	}

private:
	size_t nThreads;
	bool stop{false};
	std::mutex queueMutex;
	std::condition_variable cv;
	std::queue<std::function<void()>> tasks;
	std::vector<std::thread> workers;
};