#pragma once

#include <mutex>
#include <thread>
#include <condition_variable>

#include <luisa/core/basic_types.h>
#include <luisa/core/stl/queue.h>
#include <luisa/core/stl/functional.h>

#ifndef LUISA_COMPUTE_ENABLE_TBB
struct NanothreadPool;
#endif

namespace luisa::compute::fallback {

class FallbackCommandQueueParallelContext;
struct AkrThreadPool;
class FallbackCommandQueue {

private:
    std::mutex _mutex;
    std::condition_variable _cv;
    std::mutex _synchronize_mutex;
    std::condition_variable _synchronize_cv;
    std::thread _dispatcher;
    luisa::queue<luisa::move_only_function<void()>> _tasks;
    size_t _in_flight_limit{0u};
    std::atomic_size_t _total_enqueue_count{0u};
    std::atomic_size_t _total_finish_count{0u};
    size_t _worker_count{0u};

#ifndef LUISA_COMPUTE_ENABLE_TBB
    // NanothreadPool *_worker_pool{nullptr};
    AkrThreadPool *_worker_pool{nullptr};
#endif

private:
    void _run_dispatch_loop() noexcept;
    void _wait_for_task_queue_available() const noexcept;
    void _enqueue_task_no_wait(luisa::move_only_function<void()> &&task) noexcept;

public:
    explicit FallbackCommandQueue(size_t in_flight_limit, size_t num_threads) noexcept;
    ~FallbackCommandQueue() noexcept;
    void enqueue(luisa::move_only_function<void()> &&task) noexcept;
    void enqueue_parallel(uint n, luisa::move_only_function<void(uint)> &&task) noexcept;
    void synchronize() noexcept;
};

}// namespace luisa::compute::fallback
