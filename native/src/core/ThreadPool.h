/**
 * @file ThreadPool.h
 * @brief Lightweight thread pool with priority-based scheduling.
 */

#ifndef MAGNAUNDASONI_CORE_THREAD_POOL_H
#define MAGNAUNDASONI_CORE_THREAD_POOL_H

#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

namespace magnaundasoni {

/* ------------------------------------------------------------------ */
/* Task priority                                                      */
/* ------------------------------------------------------------------ */
enum class TaskPriority : uint32_t {
    High   = 0,
    Normal = 1,
    Low    = 2
};

/* ------------------------------------------------------------------ */
/* ThreadPool                                                         */
/* ------------------------------------------------------------------ */
class ThreadPool {
public:
    explicit ThreadPool(uint32_t threadCount = 0);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /** Submit a single task and get a future for its result. */
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

    /** Submit with explicit priority. */
    template <typename F, typename... Args>
    auto submit(TaskPriority priority, F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

    /** Submit a batch of void() tasks.  Returns when all complete. */
    void submitBatch(const std::vector<std::function<void()>>& tasks,
                     TaskPriority priority = TaskPriority::Normal);

    uint32_t threadCount() const { return static_cast<uint32_t>(workers_.size()); }

private:
    struct Task {
        std::function<void()> work;
        TaskPriority          priority = TaskPriority::Normal;
    };

    void workerLoop();

    std::vector<std::thread> workers_;
    std::deque<Task>         queue_;
    std::mutex               mutex_;
    std::condition_variable  cv_;
    bool                     stop_ = false;
};

/* ------------------------------------------------------------------ */
/* Template implementations                                           */
/* ------------------------------------------------------------------ */
template <typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>> {
    return submit(TaskPriority::Normal, std::forward<F>(f), std::forward<Args>(args)...);
}

template <typename F, typename... Args>
auto ThreadPool::submit(TaskPriority priority, F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>> {

    using ReturnType = std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<ReturnType> result = task->get_future();

    {
        std::lock_guard lock(mutex_);
        queue_.push_back({[task]() { (*task)(); }, priority});
    }
    cv_.notify_one();

    return result;
}

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_CORE_THREAD_POOL_H
