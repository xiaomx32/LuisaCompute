#ifdef LUISA_COMPUTE_ENABLE_TBB
#include <tbb/parallel_for.h>
#else
#include <nanothread/nanothread.h>
#endif
#include <luisa/core/stl/vector.h>
#include <luisa/vstl/unique_ptr.h>
#include "fallback_command_queue.h"
#include <barrier>
#include <optional>
namespace luisa::compute::fallback {

struct AkrThreadPool {
    luisa::vector<luisa::unique_ptr<std::thread>> _threads;
    std::mutex _mutex;
    std::condition_variable _has_work, _work_done;
    std::barrier<> _barrier;
    std::atomic_uint32_t _item_count = 0u;
    struct ParallelFor {
        uint count;
        uint block_size;
        luisa::move_only_function<void(uint)> task{};
    };
    std::optional<ParallelFor> _parallel_for;
    std::atomic_bool _stopped = false;
    explicit AkrThreadPool(size_t n_threads) : _barrier(static_cast<std::ptrdiff_t>(n_threads)) {
        for (size_t tid = 0; tid < n_threads; tid++) {
            _threads.emplace_back(std::move(luisa::make_unique<std::thread>([this, tid] {
                while (!_stopped.load(std::memory_order_relaxed)) {
                    std::unique_lock lock{_mutex};
                    _has_work.wait(lock, [this] { return _parallel_for.has_value() || _stopped.load(std::memory_order_relaxed); });
                    if (_stopped.load(std::memory_order_relaxed)) { return; }
                    auto &&[count, block_size, task] = *_parallel_for;
                    lock.unlock();
                    // std::printf("thread %zu start working on %u items\n", tid, count);
                    while (_item_count < count) {
                        auto i = _item_count.fetch_add(block_size, std::memory_order_relaxed);
                        for (uint j = i; j < std::min<uint>(i + block_size, count); j++) {
                            task(j);
                        }
                    }
                    // std::printf("thread %zu finish working on %u items\n", tid, count);
                    _barrier.arrive_and_wait();
                    if (tid == 0) {
                        _work_done.notify_all();
                        _parallel_for.reset();
                    }
                    _barrier.arrive_and_wait();
                }
            })));
        }
    }
    void parallel_for(uint count, luisa::move_only_function<void(uint)> &&task) {
        std::unique_lock lock{_mutex};
        _parallel_for = ParallelFor{count, 1u, std::move(task)};
        _item_count.store(0, std::memory_order_seq_cst);
        // std::printf("notify all\n");
        _has_work.notify_all();
        _work_done.wait(lock);//, [this] { return _item_count.load(std::memory_order_seq_cst) == _parallel_for->count; });
        // std::printf("work done\n");
    }
};

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
#ifndef LUISA_COMPUTE_ENABLE_TBB
    if (_worker_pool != nullptr) {
        // nanothread_pool_destroy(_worker_pool);
        delete _worker_pool;
    }
#endif
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
#ifndef LUISA_COMPUTE_ENABLE_TBB
        if (_worker_pool == nullptr) {
            // _worker_pool = nanothread_pool_create(_worker_count);
            _worker_pool = new AkrThreadPool(_worker_count);
        }
        _worker_pool->parallel_for(n, std::move(task));
        // drjit::blocked_range<uint> range{0, n, 16};
        // drjit::parallel_for(range, [&task](drjit::blocked_range<uint> r) noexcept {
        //     for (auto i : r) {
        //         task(i);
        //     } }, _worker_pool);
#else
        tbb::parallel_for(0u, n, task);
#endif
    });
}

}// namespace luisa::compute::fallback
