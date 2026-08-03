#pragma once

#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(std::size_t num_threads) : num_threads_(num_threads) { Init(); }

    ThreadPool() : num_threads_(std::thread::hardware_concurrency()) {
        num_threads_ == 0 ? num_threads_ = 1 : num_threads_;
        Init();
    }

    ~ThreadPool() {
        if (running_)
            Shutdown();

        for (auto& t : threads_) {
            if (t.joinable())
                t.join();
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    void Init() {
        for (std::size_t i = 0; i < num_threads_; ++i)
            threads_.emplace_back(&ThreadPool::LaunchThread, this);
    }

    void LaunchThread() {
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

    void Shutdown() {
        running_ = false;
        condition_.notify_all();
    }

    template <typename Fn, typename... Args>
        requires std::invocable<Fn, Args...>
    [[nodiscard]] std::future<std::invoke_result_t<Fn, Args...>> Enqueue(Fn&& func, Args&&... args) {

        if (!running_) 
            throw std::logic_error("Task enqueued after shutdown");
        
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

    template <typename Fn, typename... Args>
        requires std::invocable<Fn, Args...>
    void EnqueueDetached(Fn&& func, Args&&... args) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.push([f = std::forward<Fn>(func), ... a = std::forward<Args>(args)]() mutable {
                std::invoke(f, std::move(a)...);
            });
        }

        condition_.notify_one();
    }

private:
    std::atomic<bool> running_{true};
    std::size_t num_threads_{};
    std::vector<std::thread> threads_;
    std::queue<std::move_only_function<void()>> tasks_;
    std::condition_variable condition_;

    mutable std::mutex mutex_;
};
