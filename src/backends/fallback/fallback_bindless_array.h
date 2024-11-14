//
// Created by Mike Smith on 2022/2/11.
//

#pragma once

//#include <luisa/core/dirty_range.h>
//#include <luisa/core/thread_pool.h>
#include <luisa/runtime/rhi/sampler.h>
#include "../common/resource_tracker.h"
#include "fallback_texture.h"

namespace luisa
{
    class ThreadPool;
}

namespace luisa::compute::fallback {

class FallbackBindlessArray {

public:
    struct alignas(16) Item {
        const std::byte *buffer;
        const FallbackTexture *tex2d;
        const FallbackTexture *tex3d;
        uint sampler2d;
        uint sampler3d;
    };

    struct Slot {
        const void *buffer{nullptr};
        size_t buffer_offset{};
        Sampler sampler2d{};
        Sampler sampler3d{};
        const FallbackTexture *tex2d{nullptr};
        const FallbackTexture *tex3d{nullptr};
    };

private:
    luisa::vector<Slot> _slots{};
    luisa::vector<Item> _items{};
//    DirtyRange _dirty;
    ResourceTracker _tracker;

public:
    explicit FallbackBindlessArray(size_t capacity) noexcept;
    void emplace_buffer(size_t index, const void *buffer, size_t offset) noexcept;
    void emplace_tex2d(size_t index, const FallbackTexture *tex, Sampler sampler) noexcept;
    void emplace_tex3d(size_t index, const FallbackTexture *tex, Sampler sampler) noexcept;
    void remove_buffer(size_t index) noexcept;
    void remove_tex2d(size_t index) noexcept;
    void remove_tex3d(size_t index) noexcept;
    void update(ThreadPool &pool) noexcept;
    [[nodiscard]] bool uses_resource(uint64_t handle) const noexcept;
    [[nodiscard]] auto handle() const noexcept { return _items.data(); }
};

}// namespace luisa::compute::Fallback
