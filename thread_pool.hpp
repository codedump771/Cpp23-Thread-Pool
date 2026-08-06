#pragma once

#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// The header uses C++23-only features such as move_only_function.
// My intention is to add compatiblity with older standards in the future.

class ThreadPool {
public:
    explicit ThreadPool(std::size_t num_threads) : num_threads_(num_threads) { init(); }

    // if hardware_concurrency fails, we ensure the amount of threads is atleast one.
    ThreadPool() : num_threads_(std::max(std::thread::hardware_concurrency(), 1u)) { init(); }

    ~ThreadPool() { shutdown(); }

    // It doesn't make sense to move or copy a thread pool.
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    void wait() {
        std::unique_lock lock(mutex_);
        finished_.wait(lock, [this] { return !remaining_tasks_; });
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

        // We wrap the task inside a lambda for it to be a void function with no arguments.
        // Since we use move_only_function, we can sumbit the task to the queue directly.
        push_task([f = std::move(task)]() mutable { f(); });

        return future;
    }

    template <typename Fn, typename... Args>
        requires std::invocable<Fn, Args...>
    void enqueue_void(Fn&& func, Args&&... args) {
        auto task = [f = std::forward<Fn>(func), ... a = std::forward<Args>(args)]() mutable {
            std::invoke(f, std::move(a)...);
        };

        push_task([f = std::move(task)]() mutable { f(); });
    }

private:
    void init() {
        for (std::size_t i = 0; i < num_threads_; ++i)
            threads_.emplace_back(&ThreadPool::launch_thread, this);
    }

    template <typename Fn>
        requires std::invocable<Fn>
    void push_task(Fn&& func) {
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!running_.load())
                throw std::logic_error("Task pushed after shutdown");

            tasks_.push(std::forward<Fn>(func));
            ++remaining_tasks_;
        }
        condition_.notify_one();
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
            }
            task();

            std::lock_guard lock(mutex_);
            if (--remaining_tasks_ == 0) {
                finished_.notify_all();
            }
        }
    }
    
    std::atomic<bool> running_{true};
    std::size_t remaining_tasks_{};
    std::size_t num_threads_{};
    std::vector<std::thread> threads_;
    std::queue<std::move_only_function<void()>> tasks_;

    std::condition_variable condition_;
    std::condition_variable finished_;

    mutable std::mutex mutex_;
};
