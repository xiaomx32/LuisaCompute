//
// Created by Mike Smith on 2022/2/11.
//

#pragma once

#include <luisa/runtime/rtx/mesh.h>
#include "fallback_embree.h"

namespace luisa::compute::fallback {

class FallbackMesh {

private:
    RTCScene _handle;
    RTCGeometry _geometry;
    uint64_t _v_buffer;
    uint64_t _v_offset;
    uint64_t _v_stride;
    uint64_t _v_count;
    uint64_t _t_buffer;
    uint64_t _t_offset;
    uint64_t _t_count;
    AccelUsageHint _hint;
    std::atomic_bool _buffers_already_set{false};

public:
    FallbackMesh(RTCDevice device, AccelUsageHint hint) noexcept;
    ~FallbackMesh() noexcept;
    [[nodiscard]] auto vertex_buffer() const noexcept { return _v_buffer; }
    [[nodiscard]] auto triangle_buffer() const noexcept { return _t_buffer; }
    [[nodiscard]] auto handle() const noexcept { return _handle; }
    void commit(uint64_t v_buffer, size_t v_offset, size_t v_stride, size_t v_count,
                uint64_t t_buffer, size_t t_offset, size_t t_count) noexcept;
};

}// namespace luisa::compute::llvm