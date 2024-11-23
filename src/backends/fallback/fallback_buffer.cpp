//
// Created by swfly on 2024/11/21.
//

#include <luisa/core/logging.h>
#include "fallback_buffer.h"

namespace luisa::compute::fallback {

FallbackBufferView FallbackBuffer::view(size_t offset) noexcept {
    LUISA_ASSERT(offset <= data.size(), "Buffer view out of range.");
    return {static_cast<void *>(data.data() + offset), data.size() - offset};
}

}// namespace luisa::compute::fallback
