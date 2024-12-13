#include "fallback_command_queue.h"

namespace luisa::compute::fallback {

class FallbackCommandQueueParallelContext {

private:
    std::mutex _dispatch_mutex;
    std::condition_variable _dispatch_cv;
    luisa::move_only_function<void(uint)> _kernel;
    std::atomic_uint _fetch_count{};
    std::atomic_uint _finish_count{};
    uint _total{};
    bool _should_stop{false};

public:
    void dispatch(luisa::move_only_function<void(uint)> &&k, uint n) noexcept {
        _kernel = std::move(k);
        _fetch_count.store(0u);
        _finish_count.store(0u);
        {
            std::scoped_lock lock{_dispatch_mutex};
            _total = n;
        }
        _dispatch_cv.notify_all();
        // participate in the work
        while (process_one()) {}
        // wait for all work to finish
        while (_finish_count.load() < n) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(50us);
        }
    }

    void notify_stop() noexcept {
        {
            std::scoped_lock lock{_dispatch_mutex};
            _should_stop = true;
        }
        _dispatch_cv.notify_all();
    }

    [[nodiscard]] bool wait_for_work() noexcept {
        std::unique_lock lock{_dispatch_mutex};
        _dispatch_cv.wait(lock, [this] {
            return _total != 0u || _should_stop;
        });
        return !_should_stop;
    }

    [[nodiscard]] bool process_one() noexcept {
        auto fetch = _fetch_count.fetch_add(1u);
        if (fetch >= _total) { return false; }
        _kernel(fetch);
        _finish_count.fetch_add(1u);
        return true;
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
    if (_parallel_context) {
        _parallel_context->notify_stop();
        for (auto &w : _parallel_workers) { w.join(); }
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

inline void FallbackCommandQueue::_ensure_parallel_workers_started() noexcept {
    if (_parallel_context == nullptr) {
        _parallel_context = luisa::make_unique<FallbackCommandQueueParallelContext>();
        for (auto &w : _parallel_workers) {
            w = std::thread{[this] { _run_parallel_worker_loop(); }};
        }
    }
}

void FallbackCommandQueue::_run_parallel_worker_loop() noexcept {
    auto ctx = _parallel_context.get();
    while (ctx->wait_for_work()) {
        while (ctx->process_one()) {}
    }
}

FallbackCommandQueue::FallbackCommandQueue(size_t in_flight_limit, size_t num_threads) noexcept
    : _in_flight_limit{in_flight_limit} {
    if (num_threads == 0u) {
        num_threads = std::max<size_t>(std::thread::hardware_concurrency(), 1u);
    }
    _parallel_workers.resize(num_threads - 1u);
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
        _ensure_parallel_workers_started();
        _parallel_context->dispatch(std::move(task), n);
    });
}

}// namespace luisa::compute::fallback
