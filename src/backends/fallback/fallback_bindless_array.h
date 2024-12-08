//
// Created by Mike Smith on 2022/2/11.
//

#pragma once

//#include <luisa/core/thread_pool.h>
#include <luisa/runtime/rhi/sampler.h>
#include <luisa/runtime/rhi/command.h>
#include "../common/resource_tracker.h"
#include "fallback_texture.h"

namespace luisa
{
    class ThreadPool;
}

namespace luisa::compute::fallback {

class FallbackBindlessArray {

public:

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
    //DirtyRange _dirty;
    ResourceTracker _tracker;

public:
    explicit FallbackBindlessArray(size_t capacity) noexcept;
    [[nodiscard]] auto& slot(unsigned int idx) const noexcept { return _slots[idx]; }
    void update(ThreadPool &pool, luisa::span<const BindlessArrayUpdateCommand::Modification> modifications) noexcept;
    [[nodiscard]] bool uses_resource(uint64_t handle) const noexcept;
};

}// namespace luisa::compute::Fallback

void bindless_buffer_read(void* bindless, size_t slot, size_t elem, unsigned stride, void* buffer);
