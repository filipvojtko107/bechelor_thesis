#ifndef __THREAD_POOL_HPP__
#define __THREAD_POOL_HPP__
#include <vector>
#include <atomic>
#include <thread>
#include <functional>
#include <mutex>
#include <list>
#include <semaphore.h>


class ThreadPool
{
	using Task = std::function<void()>;

	public:
		ThreadPool();
		ThreadPool(const size_t size);
		ThreadPool(const ThreadPool& obj) = delete;
		ThreadPool(ThreadPool&& obj) = delete;
		~ThreadPool() = default;
		
		ThreadPool& operator=(const ThreadPool& obj) = delete;
		ThreadPool& operator=(ThreadPool&& obj) = delete;

		bool queueTask(const Task& task);
		bool queueTask(Task&& task);
		bool resize(const size_t size);
		bool start();
		bool stop(const bool force = false);  // Stop the threadpool without executing all tasks in the task queue
		bool reset();
		size_t size() const;
		bool isRunning() const;
		size_t busyThreads() const;
		size_t freeThreads() const;
		
	
	private:	
		void worker();
		void waitForRun() const;  // Waits untill all threads are spawned an running
		void waitForTasks() const;  // Waits until all tasks in task queue are completed
		
		
	private:
		volatile std::atomic<bool> run_;
		std::atomic<size_t> running_threads_;
		std::atomic<size_t> busy_threads_;
		std::vector<std::thread> threads_;
		std::list<Task> task_queue_;
		std::mutex mutex_;
		sem_t semaphore_;
};


inline size_t ThreadPool::size() const
{
	return threads_.size();
}


inline bool ThreadPool::isRunning() const 
{
	return run_;
}


inline size_t ThreadPool::busyThreads() const
{
	return busy_threads_;
}


inline size_t ThreadPool::freeThreads() const
{
	return (threads_.size() - busy_threads_);	
}


#endif
