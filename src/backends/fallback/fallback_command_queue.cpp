#ifdef LUISA_COMPUTE_ENABLE_TBB
#include <tbb/parallel_for.h>
#else
#include <nanothread/nanothread.h>
#endif

#include "fallback_command_queue.h"

namespace luisa::compute::fallback {

inline void FallbackCommandQueue::_run_dispatch_loop() noexcept {
    // wait and fetch tasks
    for (;;) {
        auto task = [this] {
            std::unique_lock lock{_mutex};
            _cv.wait(lock, [this] { return !_tasks.empty(); });
            auto task = std::move(_tasks.front());
            _tasks.pop();
            return task;
        }();
        // we use a null task to signal the end of the dispatcher
        if (!task) { break; }
        // good to go
        task();
        // count the finish of a task
        _total_finish_count.fetch_add(1u);
    }
    if (_worker_pool != nullptr) {
        pool_destroy(_worker_pool);
    }
}

inline void FallbackCommandQueue::_wait_for_task_queue_available() const noexcept {
    if (_in_flight_limit == 0u) { return; }
    auto last_enqueue_count = _total_enqueue_count.load();
    while (_total_finish_count.load() + _in_flight_limit <= last_enqueue_count) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(50us);
    }
}

inline void FallbackCommandQueue::_enqueue_task_no_wait(luisa::move_only_function<void()> &&task) noexcept {
    _total_enqueue_count.fetch_add(1u);
    {
        std::scoped_lock lock{_mutex};
        _tasks.push(std::move(task));
    }
    _cv.notify_one();
}

FallbackCommandQueue::FallbackCommandQueue(size_t in_flight_limit, size_t num_threads [[maybe_unused]]) noexcept
    : _in_flight_limit{in_flight_limit}, _worker_count{num_threads} {
    if (_worker_count == 0u) {
        _worker_count = std::thread::hardware_concurrency();
    }
    _dispatcher = std::thread{[this] { _run_dispatch_loop(); }};
}

FallbackCommandQueue::~FallbackCommandQueue() noexcept {
    _enqueue_task_no_wait({});// signal the end of the dispatcher
    _dispatcher.join();
}

void FallbackCommandQueue::synchronize() noexcept {
    auto finished = false;
    _enqueue_task_no_wait([this, &finished] {
        {
            std::scoped_lock lock{_synchronize_mutex};
            finished = true;
        }
        _synchronize_cv.notify_one();
    });
    std::unique_lock lock{_synchronize_mutex};
    _synchronize_cv.wait(lock, [&finished] { return finished; });
}

void FallbackCommandQueue::enqueue(luisa::move_only_function<void()> &&task) noexcept {
    _wait_for_task_queue_available();
    _enqueue_task_no_wait(std::move(task));
}

void FallbackCommandQueue::enqueue_parallel(uint n, luisa::move_only_function<void(uint)> &&task) noexcept {
    enqueue([this, n, task = std::move(task)]() mutable noexcept {
        if (_worker_pool == nullptr) {
            _worker_pool = pool_create(_worker_count);
        }
        drjit::blocked_range<int> range{0, static_cast<int>(n)};
        drjit::parallel_for(range, [&task](drjit::blocked_range<int> r) {
            for (auto i : r) {
                task(static_cast<uint>(i));
            } }, _worker_pool);
    });
}

}// namespace luisa::compute::fallback
