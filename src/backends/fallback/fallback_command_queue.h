#pragma once

#include <mutex>
#include <thread>
#include <condition_variable>

#ifdef LUISA_COMPUTE_ENABLE_TBB
#include <tbb/parallel_for.h>
#endif

#include <luisa/core/basic_types.h>
#include <luisa/core/stl/queue.h>
#include <luisa/core/stl/vector.h>
#include <luisa/core/stl/functional.h>
#include <luisa/core/stl/memory.h>

namespace luisa::compute::fallback {

class FallbackCommandQueueParallelContext;

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

#ifndef LUISA_COMPUTE_ENABLE_TBB
    luisa::vector<std::thread> _parallel_workers;
    luisa::unique_ptr<FallbackCommandQueueParallelContext> _parallel_context;
    void _ensure_parallel_workers_started() noexcept;
    void _run_parallel_worker_loop() noexcept;
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
