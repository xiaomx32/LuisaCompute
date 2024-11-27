//
// Created by swfly on 2024/11/21.
//

#include <luisa/core/logging.h>
#include "fallback_buffer.h"

namespace luisa::compute::fallback {

FallbackBufferView FallbackBuffer::view(size_t offset) noexcept {
	LUISA_ASSERT(offset <= size, "Buffer view out of range.");
    return {static_cast<void *>(data + offset * elementStride), size-offset};
}

	FallbackBuffer::FallbackBuffer(size_t size, unsigned elementStride):elementStride(elementStride), size(size)
	{
		data = luisa::allocate_with_allocator<std::byte>(size * elementStride);

	}

	FallbackBuffer::~FallbackBuffer()
	{
		luisa::deallocate_with_allocator(data);
	}

}// namespace luisa::compute::fallback
