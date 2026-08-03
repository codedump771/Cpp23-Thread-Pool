#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool {
   public:
    explicit ThreadPool(std::size_t num_threads);
    ThreadPool();

    ThreadPool(const ThreadPool& other) = delete;
    ThreadPool& operator=(const ThreadPool& other) = delete;

    ThreadPool(ThreadPool&& other);
    ThreadPool& operator=(ThreadPool&&);

    ~ThreadPool();

    void ExecuteThreadPool();
    void Shutdown();
    void LaunchThread();

    template <typename Fn, typename... Args>
        requires std::invocable<Fn, Args...>
    std::future<std::invoke_result_t<Fn, Args...>> Enqueue(Fn&& func, Args&&... args);

   private:
    bool running_{true};
    std::size_t num_threads_{};
    std::vector<std::thread> threads_;
    std::queue<std::move_only_function<void()>> tasks_;
    std::condition_variable condition_;

    mutable std::mutex mutex_;
};
