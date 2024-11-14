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
    RTCScene _handle;
    mutable luisa::vector<Instance> _instances;

private:
    [[nodiscard]] static std::array<float, 12> _compress(float4x4 m) noexcept;
    [[nodiscard]] static float4x4 _decompress(std::array<float, 12> m) noexcept;

public:
    FallbackAccel(RTCDevice device, AccelUsageHint hint) noexcept;
    ~FallbackAccel() noexcept;
    void build(ThreadPool &pool, size_t instance_count,
               luisa::span<const AccelBuildCommand::Modification> mods) noexcept;
    //    [[nodiscard]] SurfaceHit trace_closest(Ray ray) const noexcept;
    //    [[nodiscard]] bool trace_any(Ray ray) const noexcept;
    [[nodiscard]] auto handle() const noexcept { return Handle{this, _instances.data()}; }
};

[[nodiscard]] float32x4_t accel_trace_closest(const FallbackAccel *accel, int64_t r0, int64_t r1, int64_t r2, int64_t r3) noexcept;
[[nodiscard]] bool accel_trace_any(const FallbackAccel *accel, int64_t r0, int64_t r1, int64_t r2, int64_t r3) noexcept;

struct alignas(16) FallbackAccelInstance {
    float affine[12];
    bool visible;
    bool dirty;
    uint pad;
    uint geom0;
    uint geom1;
};

}// namespace luisa::compute::fallback

LUISA_STRUCT(luisa::compute::fallback::FallbackAccelInstance,
             affine, visible, dirty, pad, geom0, geom1) {};
