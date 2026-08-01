#include <thread>
#include <mutex>
#include <future>
#include <vector>
#include <queue>

class ThreadPool {
public: 
    explicit ThreadPool(std::size_t num_threads);
    void ExecuteThreadPool();
    void LaunchThread();
private: 
    bool running_{true};
    std::size_t num_threads_{};
    std::vector<std::thread> threads_;
    std::queue<std::packaged_task<void()>> tasks_;
    std::condition_variable condition_;
    
    mutable std::mutex mutex_;
};