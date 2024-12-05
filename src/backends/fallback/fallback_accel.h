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
#include "llvm_abi.h"

namespace luisa {
class ThreadPool;
}// namespace luisa

namespace luisa::compute::fallback {

class FallbackMesh;

class FallbackAccel {

public:
    struct alignas(16) Instance {
        float affine[12];
        bool visible;
        bool dirty;
        uint pad;
        RTCGeometry geometry;
    };
    static_assert(sizeof(Instance) == 64u);

    struct alignas(16) Handle {
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
	[[nodiscard]]auto device()const noexcept{return _device;}
    [[nodiscard]] RTCScene scene()const noexcept {return _handle;}
    FallbackAccel(RTCDevice device, AccelUsageHint hint) noexcept;
    ~FallbackAccel() noexcept;
    void build(ThreadPool &pool, size_t instance_count,
               luisa::span<const AccelBuildCommand::Modification> mods) noexcept;
    [[nodiscard]] auto handle() const noexcept { return Handle{this, _instances.data()}; }
};

void accel_trace_closest(const FallbackAccel *accel, float ox, float oy, float oz, float dx, float dy, float dz, float tmin, float tmax, uint mask, SurfaceHit* hit) noexcept;
[[nodiscard]] bool accel_trace_any(const FallbackAccel *accel, float ox, float oy, float oz, float dx, float dy, float dz, float tmin, float tmax, uint mask) noexcept;
void fill_transform(const FallbackAccel* accel, uint id, float4x4* buffer);
}// namespace luisa::compute::fallback


void intersect_closest_wrapper(void *accel, float ox, float oy, float oz, float dx, float dy, float dz, float tmin, float tmax, unsigned mask, void *hit);
void accel_transform_wrapper(void *accel, unsigned id, void* buffer);
