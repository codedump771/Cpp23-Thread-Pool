#pragma once

#include "tasks_queue.hpp"

#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

// The header uses C++23-only features such as move_only_function.
// My intention is to add compatiblity with older standards in the future.

class ThreadPool {
public:

    using FnType = std::move_only_function<void()>;
    
    explicit ThreadPool(std::size_t num_threads = std::max(std::thread::hardware_concurrency(), 1u)) 
    : num_threads_(num_threads), queues_(num_threads) { init(); }

    // if hardware_concurrency fails, we ensure the amount of threads is atleast one.

    ~ThreadPool() { shutdown(); }

    // It doesn't make sense to move or copy a thread pool.
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    void shutdown() {
        if (!running_.exchange(false, std::memory_order_acquire))
            return;
        
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

        if (!running_.load(std::memory_order_acquire))
            throw std::runtime_error("enqueue() was called after shut down.");
        
        std::packaged_task<ReturnType()> task([f = std::forward<Fn>(func), ... a = std::forward<Args>(args)]() mutable {
            try {
                return std::invoke(f, std::move(a)...);
            }
            catch (...) {
                throw std::current_exception();
            }
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

        if (!running_.load(std::memory_order_acquire))
            throw std::runtime_error("enqueue_void() was called after shutdown");
        
        auto task = [f = std::forward<Fn>(func), ... a = std::forward<Args>(args)]() mutable {
            std::invoke(f, std::move(a)...);
        };

        push_task([f = std::move(task)]() mutable { f(); });
    }

    void wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        wait_.wait(lock, [this] {
            return remaining_tasks_.load(std::memory_order_acquire) == 0; 
        });
    }
    
private:
    void init() {
        for (std::size_t i = 0; i < num_threads_; ++i) {
            threads_.emplace_back(&ThreadPool::launch_thread, this, i, std::ref(queues_[i]));
        }
    }

    template <typename Fn>
        requires std::invocable<Fn>
    void push_task(Fn&& func) {
        {
            
            std::lock_guard<std::mutex> lock(mutex_);

            if (!running_.load(std::memory_order_acquire))
                throw std::logic_error("Task pushed after shutdown");

            std::size_t id = count_.fetch_add(1, std::memory_order_relaxed) % queues_.size();
            
            queues_[id].push(std::forward<Fn>(func));
            remaining_tasks_.fetch_add(1, std::memory_order_release);
        }
        condition_.notify_one();
    }

    std::optional<FnType> try_steal(std::size_t id) {
        
        for (std::size_t i = 0; i < queues_.size(); ++i) {
            const std::size_t idx = (id + i + 1) % queues_.size();
            if (auto stolen_task = queues_[idx].try_steal()) {
                return stolen_task;
            }
        }

        return std::nullopt;
    }

    void launch_thread(std::size_t id, TasksQueue& local_queue) {

        while (true) {

            FnType task;
            {   
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this] { 
                    return !running_.load(std::memory_order_acquire) || 
                    remaining_tasks_.load(std::memory_order_acquire) > 0;
                });

                if (!running_.load(std::memory_order_acquire) && remaining_tasks_.load(std::memory_order_acquire) == 0)
                    // Ensure all tasks are finished before stopping
                    return;

                if (auto opt_task = local_queue.try_pop())
                    task = std::move(*opt_task);
                else if (opt_task = try_steal(id)) //NOLINT
                    task = std::move(*opt_task);
                else 
                    continue;
            }
            task();
            
            if (remaining_tasks_.fetch_sub(1) == 1) {
                wait_.notify_one();
            }
        }
    }
    
    std::atomic<bool> running_{true};
    std::atomic<std::size_t> remaining_tasks_{};
    std::size_t num_threads_{};
    std::atomic<size_t> count_{};
    std::vector<std::thread> threads_;
    
    std::vector<TasksQueue> queues_;
    
    std::condition_variable condition_;
    std::condition_variable wait_;

    mutable std::mutex mutex_;
};
