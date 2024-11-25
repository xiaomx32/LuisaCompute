//
// Created by swfly on 2024/11/21.
//

#pragma once

#include <vector>

namespace luisa::compute::fallback {
struct alignas(16) FallbackBufferView {
    void *ptr;
    size_t size;
};

class FallbackBuffer {
public:
    // FIXME: size
    void *addr() { return data.data(); }
    [[nodiscard]] FallbackBufferView view(size_t offset) noexcept;

private:

    std::vector<std::byte> data{};
};

}// namespace luisa::compute::fallback
