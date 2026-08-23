#pragma once

#include <deque>
#include <functional>
#include <mutex>

constexpr std::size_t cache_padding {std::hardware_destructive_interference_size};

class alignas(cache_padding) TasksQueue {
private:
    using FnType = std::move_only_function<void()>;
    std::deque<FnType> queue_;
    mutable std::mutex mutex_;
public: 
    TasksQueue() {}
    
    TasksQueue(const TasksQueue&) = delete;
    TasksQueue& operator=(const TasksQueue&) = delete;

    void push(FnType& func) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_front(std::move(func));
    }

    [[nodiscard]] FnType pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        FnType func = std::move(queue_.front());
        queue_.pop_front();

        return func;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    bool try_pop(FnType& func) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        func = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    bool try_steal(FnType& func) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        func = std::move(queue_.back());
        queue_.pop_back();
        return true;
    }
};