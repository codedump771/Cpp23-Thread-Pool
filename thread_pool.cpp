#include "thread_pool.hpp"

#include <type_traits>
ThreadPool::ThreadPool(std::size_t num_threads) : num_threads_(num_threads) { ExecuteThreadPool(); }

ThreadPool::ThreadPool() : num_threads_(std::thread::hardware_concurrency()) {
    if (num_threads_ == 0) num_threads_ = 1;
}

void ThreadPool::ExecuteThreadPool() {
    for (std::size_t i = 0; i < num_threads_; ++i) {
        threads_.emplace_back(&ThreadPool::LaunchThread, this);
    }
}

void ThreadPool::LaunchThread() {
    while (true) {
        std::move_only_function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] { return !running_ || !tasks_.empty(); });
            if (!running_ && tasks_.empty())
                // Ensure all tasks are finished before stopping
                return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}

template <typename Fn, typename... Args>
    requires std::invocable<Fn, Args...>
[[nodiscard]] std::future<std::invoke_result_t<Fn, Args...>> ThreadPool::Enqueue(Fn&& func, Args&&... args) {
    using ReturnType = std::invoke_result_t<Fn, Args...>;

    std::packaged_task<ReturnType()> task([f = std::forward<Fn>(func), ... a = std::forward<Args>(args)]() mutable {
        return std::invoke(f, std::move(a)...);
    });

    std::future<ReturnType> future = task.get_future();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push([t = std::move(task)] mutable { t(); });
    }

    condition_.notify_one();
    return future;
}

void ThreadPool::Shutdown() {
    for (auto& t : threads_) t.join();

    condition_.notify_all();
}
