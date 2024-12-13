#pragma once

#include <mutex>
#include <thread>
#include <condition_variable>

#include <luisa/core/basic_types.h>
#include <luisa/core/stl/queue.h>
#include <luisa/core/stl/vector.h>
#include <luisa/core/stl/functional.h>

namespace luisa::compute::fallback {

class FallbackCommandQueue {

private:
    std::mutex _mutex;
    std::condition_variable _cv;
    std::thread _dispatcher;
    luisa::vector<std::thread> _kernel_workers;
    luisa::queue<luisa::move_only_function<void()>> _tasks;
    size_t _in_flight_limit{0u};
    std::atomic_size_t _total_enqueue_count{0u};
    std::atomic_size_t _total_finish_count{0u};

private:
    void _run_dispatch_loop(size_t num_threads) noexcept {
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
    }

    void _wait_for_task_queue_available() noexcept {
        auto last_enqueue_count = _total_enqueue_count.load();
        while (_total_finish_count.load() + _in_flight_limit <= last_enqueue_count) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(50us);
        }
    }

    void _enqueue_task_no_wait(luisa::move_only_function<void()> &&task) noexcept {
        _total_enqueue_count.fetch_add(1u);
        {
            std::scoped_lock lock{_mutex};
            _tasks.push(std::move(task));
        }
        _cv.notify_one();
    }

public:
    explicit FallbackCommandQueue(size_t in_flight_limit, size_t num_threads) noexcept
        : _in_flight_limit{in_flight_limit} {
        if (num_threads == 0u) {
            num_threads = std::max<size_t>(std::thread::hardware_concurrency(), 1u);
        }
        _dispatcher = std::thread{[this, num_threads] {
            _run_dispatch_loop(num_threads);
        }};
    }

    ~FallbackCommandQueue() noexcept {
        _enqueue_task_no_wait({});// signal the end of the dispatcher
        _dispatcher.join();
    }

    void enqueue(luisa::move_only_function<void()> &&task) noexcept {
        _wait_for_task_queue_available();
        _enqueue_task_no_wait(std::move(task));
    }

    void enqueue_parallel(uint n, luisa::move_only_function<void(uint)> &&task) noexcept {
        enqueue([n, task = std::move(task)] {
            // TODO
            for (uint i = 0u; i < n; i++) { task(i); }
        });
    }

    void synchronize() noexcept {
        auto last_enqueue_count = _total_enqueue_count.load();
        while (_total_finish_count.load() < last_enqueue_count) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(50us);
        }
    }
};

}// namespace luisa::compute::fallback