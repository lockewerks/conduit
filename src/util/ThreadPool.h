#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>

namespace conduit {

// a basic thread pool because std::async is a lie
// (it doesn't actually guarantee parallelism on MSVC, ask me how i know)
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = 4);
    ~ThreadPool();

    // submit work and get a future back
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using ReturnType = decltype(f(args...));
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        auto future = task->get_future();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopped_) {
                throw std::runtime_error("submitting to a dead thread pool, bold move");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        cv_.notify_one();
        return future;
    }

    // fire and forget version
    void enqueue(std::function<void()> task);

    size_t threadCount() const { return threads_.size(); }

private:
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopped_ = false;
};

} // namespace conduit
