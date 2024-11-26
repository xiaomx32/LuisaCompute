//
// Created by Mike Smith on 2022/2/11.
//

#include "fallback_bindless_array.h"
#include "thread_pool.h"
#include "luisa/runtime/rtx/triangle.h"
#include "luisa/rust/api_types.hpp"

namespace luisa::compute::fallback
{
    FallbackBindlessArray::FallbackBindlessArray(size_t capacity) noexcept
        : _slots(capacity), _tracker{capacity}
    {
    }


    void FallbackBindlessArray::update(ThreadPool& pool, luisa::span<const BindlessArrayUpdateCommand::Modification> modifications) noexcept
    {
        using Mod = BindlessArrayUpdateCommand::Modification;
        pool.async([this,mods = luisa::vector<Mod>{modifications.cbegin(), modifications.cend()}]
        {
            for (auto m: mods)
            {
                _slots[m.slot].buffer = reinterpret_cast<void*>(m.buffer.handle);
            }
        });
        _tracker.commit();
    }

    bool FallbackBindlessArray::uses_resource(uint64_t handle) const noexcept
    {
        return _tracker.contains(handle);
    }
} // namespace luisa::compute::Fallback
void bindless_buffer_read(void* bindless, size_t slot, size_t elem, unsigned stride, void* buffer)
{
    auto a = reinterpret_cast<luisa::compute::fallback::FallbackBindlessArray *>(bindless);
    auto ptr = reinterpret_cast<const std::byte*>(a->slot(slot).buffer);
    std::memcpy(buffer, ptr + elem*stride, stride);
}
