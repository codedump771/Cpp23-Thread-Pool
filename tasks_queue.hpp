#pragma once

#include <deque>
#include <functional>
#include <mutex>
#include <optional>

namespace detail {
    
constexpr std::size_t cache_padding {std::hardware_destructive_interference_size};

}

class alignas(detail::cache_padding) TasksQueue { 
private:
    using FnType = std::move_only_function<void()>;
    std::deque<FnType> queue_;
    mutable std::mutex mutex_;
public: 
    TasksQueue() {}
    
    TasksQueue(const TasksQueue&) = delete;
    TasksQueue& operator=(const TasksQueue&) = delete;

    void push(FnType&& func) {
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

    [[nodiscard]] std::optional<FnType> try_pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        std::optional<FnType> func = std::move(queue_.front());
        queue_.pop_front();
        return func;
    }

    [[nodiscard]] std::optional<FnType> try_steal() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        std::optional<FnType> func = std::move(queue_.back());
        queue_.pop_back();
        return func;
    }
};