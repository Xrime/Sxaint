//
// Created by xint2 on 03/07/2026.
//

#ifndef SXAINT_THREAD_POOL_H
#define SXAINT_THREAD_POOL_H
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <future>
#include <semaphore>
#include <atomic>

namespace sxaint::core {
    class ThreadPool {
    public:
        explicit ThreadPool(size_t threads =0);
        ~ThreadPool();
        template<typename F>
        auto submit(F&& f) -> std::future<std::invoke_result_t<F>> {
            using return_type = std::invoke_result_t<F>;
            auto task = std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(f));
            std::future<return_type> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                if (shutdown_) {
                    throw std::runtime_error("cannot submit to a stopped ThreadPool");
                }
                tasks_.emplace([task]() {(*task)();});
                queuedTasks_++;
            }
            taskSemaphore_.release();
            return res;
        }
        void shutdown();
        size_t active_threads() const;
        size_t queuedTask() const;

    private:
        std::vector<std::jthread> workers_;
        std::queue<std::function<void()>> tasks_;
        std::mutex queue_mutex_;
        std::counting_semaphore<> taskSemaphore_{0};
        std::atomic<bool> shutdown_{false};
        std::atomic<size_t> activeTasks_{0};
        std::atomic<size_t> queuedTasks_{0};


    };
}
#endif //SXAINT_THREAD_POOL_H