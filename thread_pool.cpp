#include "thread_pool.hpp"

ThreadPool::ThreadPool(std::size_t num_threads) : num_threads_(num_threads) {
    ExecuteThreadPool();
}

void ThreadPool::ExecuteThreadPool() {
    for (std::size_t i = 0; i < num_threads_; ++i) {
        threads_.emplace_back(&ThreadPool::LaunchThread, this);
    }
}

void ThreadPool::LaunchThread() {
    while (true) {
        std::packaged_task<void()> task; 
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_ && tasks_.empty()) // Ensure all tasks are finished before stopping to run
                return;
            task = std::move(tasks_.front());
            tasks_.pop(); 
        }
        task();
    }
}

// I like this project