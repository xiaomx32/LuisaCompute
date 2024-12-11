//
// Created by Mike Smith on 2022/2/11.
//

#pragma once

#include <luisa/core/stl.h>
#include <luisa/runtime/rtx/accel.h>
#include <luisa/dsl/struct.h>
#include <luisa/runtime/rtx/ray.h>
#include <luisa/runtime/rtx/hit.h>

#include "fallback_embree.h"

namespace luisa {
class ThreadPool;
}// namespace luisa

namespace luisa::compute::fallback {

class FallbackMesh;

class alignas(16) FallbackAccel {

public:
    struct alignas(16) Instance {
        float affine[12];
        uint8_t mask;
        bool opaque;
        bool dirty;
        uint user_id;
        RTCGeometry geometry;
    };
    static_assert(sizeof(Instance) == 64u);

    struct alignas(16) View {
        const FallbackAccel *accel;
        Instance *instances;
    };

private:
    const RTCDevice _device;
    RTCScene _handle;
    mutable luisa::vector<Instance> _instances;

private:
    [[nodiscard]] static std::array<float, 12> _compress(float4x4 m) noexcept;
    [[nodiscard]] static float4x4 _decompress(std::array<float, 12> m) noexcept;

public:
    [[nodiscard]] auto device() const noexcept { return _device; }
    [[nodiscard]] RTCScene scene() const noexcept { return _handle; }
    FallbackAccel(RTCDevice device, AccelUsageHint hint) noexcept;
    ~FallbackAccel() noexcept;
    void build(ThreadPool &pool, size_t instance_count,
               luisa::vector<AccelBuildCommand::Modification> &&mods) noexcept;
    [[nodiscard]] auto view() const noexcept { return View{this, _instances.data()}; }
};

using FallbackAccelView = FallbackAccel::View;

}// namespace luisa::compute::fallback
