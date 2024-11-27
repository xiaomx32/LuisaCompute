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
	explicit FallbackBuffer(size_t size, unsigned elementStride);
    void *addr() { return data; }
    [[nodiscard]] FallbackBufferView view(size_t offset) noexcept;
	~FallbackBuffer();
private:

	unsigned elementStride;
	unsigned size;
    std::byte* data{};
};

}// namespace luisa::compute::fallback
