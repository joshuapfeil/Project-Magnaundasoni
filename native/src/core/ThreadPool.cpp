/**
 * @file ThreadPool.cpp
 * @brief Thread pool with priority-ordered task queue.
 */

#include "core/ThreadPool.h"

#include <algorithm>

namespace magnaundasoni {

/* ------------------------------------------------------------------ */
/* Construction / Destruction                                         */
/* ------------------------------------------------------------------ */
ThreadPool::ThreadPool(uint32_t threadCount) {
    if (threadCount == 0) {
        threadCount = std::max(1u, std::thread::hardware_concurrency());
    }

    workers_.reserve(threadCount);
    for (uint32_t i = 0; i < threadCount; ++i) {
        workers_.emplace_back(&ThreadPool::workerLoop, this);
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
}

/* ------------------------------------------------------------------ */
/* Worker loop                                                        */
/* ------------------------------------------------------------------ */
void ThreadPool::workerLoop() {
    for (;;) {
        Task task;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });

            if (stop_ && queue_.empty()) return;

            // Pick the highest-priority task (lowest enum value).
            auto bestIt = queue_.begin();
            for (auto it = queue_.begin(); it != queue_.end(); ++it) {
                if (it->priority < bestIt->priority) bestIt = it;
            }

            task = std::move(*bestIt);
            queue_.erase(bestIt);
        }

        task.work();
    }
}

/* ------------------------------------------------------------------ */
/* Batch submission                                                   */
/* ------------------------------------------------------------------ */
void ThreadPool::submitBatch(const std::vector<std::function<void()>>& tasks,
                              TaskPriority priority) {
    std::vector<std::future<void>> futures;
    futures.reserve(tasks.size());

    for (const auto& fn : tasks) {
        futures.push_back(submit(priority, fn));
    }

    for (auto& f : futures) {
        f.get();
    }
}

} // namespace magnaundasoni
