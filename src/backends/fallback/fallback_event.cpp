#include <thread>
#include <chrono>

#include "fallback_event.h"

namespace luisa::compute::fallback {

void FallbackEvent::signal(uint64_t fence_value) noexcept {
    using A = _Atomic(uint64_t);
    auto p = reinterpret_cast<A *>(&_fence_value);
    __c11_atomic_fetch_max(p, fence_value, __ATOMIC_RELEASE);
}

void FallbackEvent::wait(uint64_t fence_value) const noexcept {
    // spin until the fence value is reached
    while (!is_completed(fence_value)) {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(50us);
    }
}

}// namespace luisa::compute::fallback
