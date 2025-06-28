#include "ThreadPool.hpp"
#include "Logger.hpp"
#include <string.h>


ThreadPool::ThreadPool() :
	run_(false),
	running_threads_ (0),
	busy_threads_(0),
	semaphore_({0})
{

}

ThreadPool::ThreadPool(const size_t size) :
	run_(false),
	running_threads_ (0),
	busy_threads_(0),
	threads_(size),
	semaphore_({0})
{

}


bool ThreadPool::resize(const size_t size)
{
	if (!run_) 
	{
		threads_ = std::vector<std::thread>(size);
		return true;
	}
	
	return false;
}


bool ThreadPool::start()
{	
	try
	{
		if (!run_)
		{	
			if (sem_init(&semaphore_, 0, 0) == -1) 
			{
				LOG_ERR("Failed to init thread pool");
				return false;
			}

			run_ = true;
			for (auto& thread : threads_) {
				thread = std::thread(&ThreadPool::worker, this);
			}

			waitForRun();
			return true;
		}
	}
	
	catch (const std::exception& e)
	{
		LOG_ERR("Failed to spawn threads");
		stop();
	}
	
	return false;
}


void ThreadPool::waitForRun() const
{
	const size_t threads_size = threads_.size();
	while (running_threads_ != threads_size);
}


void ThreadPool::waitForTasks() const
{
	while (!task_queue_.empty());
}


bool ThreadPool::stop(const bool force)
{		
	try
	{
		bool ret = true;
		std::lock_guard<std::mutex> lock(mutex_);
		
		if (run_)
		{
			if (!force) {
				waitForTasks();
			}

			run_ = false;
			
			for (size_t i = 0; i < threads_.size(); ++i) 
			{
				if (sem_post(&semaphore_) == -1) 
				{
					//LOG_DBG("Failed to stop thread pool thread");
					ret = false;
				}
			}

			for (auto& thread : threads_) 
			{
				if (thread.joinable()) 
				{
					thread.join();
				}
			}

			sem_destroy(&semaphore_);
			return ret;
		}
	}
	
	catch (const std::exception& e) {
		LOG_ERR("Failed to stop thread pool (error: %s)", e.what());
	}
	
	return false;
}


bool ThreadPool::reset()
{
	if (!run_) 
	{
		running_threads_ = 0;
		busy_threads_ = 0;
		threads_.clear();
		task_queue_.clear();
		memset(&semaphore_, 0, sizeof(semaphore_));
		return true;
	}
	
	return false;
}


bool ThreadPool::queueTask(const Task& task)
{
	try
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (run_)
		{
			//LOG_DBG("Task queuing...");

			task_queue_.push_back(task);
			if (sem_post(&semaphore_) == -1)
			{
				//LOG_DBG("Failed to wake up thread (sem_post())");
				task_queue_.pop_back();
				return false;
			}
			
			//LOG_DBG("Task queued");
			return true;
		}
	}

	catch (const std::exception& e) {
		//LOG_DBG("Failed to queue task (error: %s)", e.what());
	}
	
	return false;
}


bool ThreadPool::queueTask(Task&& task)
{
	try
	{
		//LOG_DBG("Calling queueTask()...");

		std::lock_guard<std::mutex> lock(mutex_);
		if (run_)
		{
			//LOG_DBG("Task queuing...");

			task_queue_.emplace_back(std::move(task));
			if (sem_post(&semaphore_) == -1)
			{
				//LOG_DBG("Failed to wake up thread (sem_post())");
				task_queue_.pop_back();
				return false;
			}
			
			//LOG_DBG("Task queued");
			return true;
		}
	}

	catch (const std::exception& e) {
		//LOG_DBG("Failed to queue task (error: %s)", e.what());
	}
	
	return false;
}


void ThreadPool::worker()
{
	Task task;
	running_threads_ += 1;
	
	while (run_)
	{
		if (sem_wait(&semaphore_) == -1) 
		{
			//LOG_DBG("Thread synchronization failed");
			continue;
		}

		// Kontrola po probuzeni vlakna
		if (!run_) {
			break;
		}
		
		mutex_.lock();
		if (!task_queue_.empty())
		{
			task = std::move(task_queue_.front());
			task_queue_.pop_front();
			mutex_.unlock();
		
			if (task) 
			{
				busy_threads_ += 1;
				task();
				busy_threads_ -= 1;
			}
		}
		
		else {
			mutex_.unlock();
		}
	}
	
	running_threads_ -= 1;
}

