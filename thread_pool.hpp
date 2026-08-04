#pragma once

#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(std::size_t num_threads) : num_threads_(num_threads) { init(); }

    ThreadPool() : num_threads_(std::thread::hardware_concurrency()) { init(); }

    ~ThreadPool() {
        shutdown();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    void init() {
        for (std::size_t i = 0; i < num_threads_; ++i)
            threads_.emplace_back(&ThreadPool::launch_thread, this);
    }

    void launch_thread() {
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
                remaining_tasks_--;
            }
            task();
        }
    }

    void wait() {
        while (!threads_.empty()) {
            
        }
    }

    void shutdown() {
        if (!running_.load())
            return;

        running_.store(false);
        
        condition_.notify_all();

        for (auto& t : threads_) {
            if (t.joinable())
                t.join();
        }
    }

    template <typename Fn, typename... Args>
        requires std::invocable<Fn, Args...>
    [[nodiscard]] std::future<std::invoke_result_t<Fn, Args...>> enqueue(Fn&& func, Args&&... args) {
        using ReturnType = std::invoke_result_t<Fn, Args...>;

        std::packaged_task<ReturnType()> task([f = std::forward<Fn>(func), ... a = std::forward<Args>(args)]() mutable {
            return std::invoke(f, std::move(a)...);
        });

        std::future<ReturnType> future = task.get_future();

        push_task([f = std::move(task)]() mutable { f(); });

        return future;
    }

    template <typename Fn, typename... Args>
        requires std::invocable<Fn, Args...>
    void enqueue_void(Fn&& func, Args&&... args) {
        push_task([f = std::forward<Fn>(func), ... a = std::forward<Args>(args)]() mutable {
            std::invoke(f, std::move(a)...);
        });
    }

private:
    template <typename Fn>
        requires std::invocable<Fn>
    void push_task(Fn&& func) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.push(std::forward<Fn>(func));
            remaining_tasks_++;
        }
        condition_.notify_one();
    }

    std::atomic<bool> running_{true};
    std::size_t remaining_tasks_{};
    std::size_t num_threads_{};
    std::vector<std::thread> threads_;
    std::queue<std::move_only_function<void()>> tasks_;
    std::condition_variable condition_;

    mutable std::mutex mutex_;
};
