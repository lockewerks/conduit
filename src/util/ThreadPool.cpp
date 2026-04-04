#include "util/ThreadPool.h"
#include "util/Logger.h"

namespace conduit {

ThreadPool::ThreadPool(size_t num_threads) {
    for (size_t i = 0; i < num_threads; i++) {
        threads_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cv_.wait(lock, [this] { return stopped_ || !tasks_.empty(); });
                    if (stopped_ && tasks_.empty()) return;
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                try {
                    task();
                } catch (const std::exception& e) {
                    LOG_ERROR(std::string("thread pool task exploded: ") + e.what());
                }
            }
        });
    }
    LOG_DEBUG("thread pool started with " + std::to_string(num_threads) + " workers");
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    cv_.notify_all();
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) return;
        tasks_.emplace(std::move(task));
    }
    cv_.notify_one();
}

} // namespace conduit
