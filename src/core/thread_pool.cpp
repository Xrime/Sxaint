//
// Created by xint2 on 03/07/2026.
//
#include "../include/core/thread_pool.h"
#include <spdlog/spdlog.h>
#include <windows.h>

namespace sxaint::core {
    ThreadPool::ThreadPool(size_t threads) {
        if (threads==0) {
            threads = std::thread::hardware_concurrency();
            threads = threads > 0 ? threads*2:8; //2x-4x

        }
        spdlog::debug("Starting ThreadPool with {} workers", threads);
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this, i](std::stop_token stoken) {
                SetThreadAffinityMask(GetCurrentThread(), 1ULL << (i % std::thread::hardware_concurrency()));
                while (!stoken.stop_requested()) {
                    taskSemaphore_.acquire();
                    if (shutdown_ && tasks_.empty()) return;
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex>lock(queue_mutex_);
                        if (tasks_.empty()) continue;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                        queuedTasks_--;
                    }
                    activeTasks_++;
                    try {
                        task();
                    }catch (const std::exception& e) {
                        spdlog::error("Thread worker caught exception: {}", e.what());
                    }
                    activeTasks_--;
                }
            });
        }
    }
    ThreadPool::~ThreadPool() {
        shutdown();
    }
    void ThreadPool::shutdown() {
        if (shutdown_.exchange(true)) return;
        spdlog::debug("Turning Off ThreadPool...");
        taskSemaphore_.release(workers_.size());
        workers_.clear();
    }
    size_t ThreadPool::active_threads() const {
        return activeTasks_.load();
    }
    size_t ThreadPool::queuedTask() const {
        return queuedTasks_.load();
    }

}