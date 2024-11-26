//
// Created by Mike Smith on 2022/2/11.
//

#include <luisa/core/stl.h>

#include "fallback_mesh.h"
#include "fallback_accel.h"
#include "thread_pool.h"

namespace luisa::compute::fallback
{
    FallbackAccel::FallbackAccel(RTCDevice device, AccelUsageHint hint) noexcept
        : _handle{rtcNewScene(device)}, _device(device)
    {
        switch (hint)
        {
            case AccelUsageHint::FAST_TRACE:
                rtcSetSceneBuildQuality(_handle, RTC_BUILD_QUALITY_HIGH);
                rtcSetSceneFlags(_handle, RTC_SCENE_FLAG_COMPACT);
                break;
            case AccelUsageHint::FAST_BUILD:
                rtcSetSceneBuildQuality(_handle, RTC_BUILD_QUALITY_LOW);
                rtcSetSceneFlags(_handle, RTC_SCENE_FLAG_DYNAMIC);
                break;
        }
    }

    FallbackAccel::~FallbackAccel() noexcept { rtcReleaseScene(_handle); }

    void FallbackAccel::build(ThreadPool& pool, size_t instance_count,
                              luisa::span<const AccelBuildCommand::Modification> mods) noexcept
    {
        using Mod = AccelBuildCommand::Modification;
        pool.async([this, instance_count, mods = luisa::vector<Mod>{mods.cbegin(), mods.cend()}]()
        {
            if (instance_count < _instances.size())
            {
                // remove redundant geometries
                for (auto i = instance_count; i < _instances.size(); i++) { rtcDetachGeometry(_handle, i); }
                _instances.resize(instance_count);
            }
            else
            {
                // create new geometries
                auto device = rtcGetSceneDevice(_handle);
                _instances.reserve(next_pow2(instance_count));
                for (auto i = _instances.size(); i < instance_count; i++)
                {
                    auto geometry = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_INSTANCE);
                    rtcSetGeometryBuildQuality(geometry, RTC_BUILD_QUALITY_HIGH);
                    rtcAttachGeometryByID(_handle, geometry, i);
                    rtcReleaseGeometry(geometry); // already moved into the scene
                    _instances.emplace_back().geometry = geometry;
                }
            }
            for (auto m: mods)
            {
                auto geometry = _instances[m.index].geometry;
                if (m.flags & Mod::flag_primitive)
                {
                    auto fbMesh = reinterpret_cast<const FallbackMesh *>(m.primitive);
                    rtcSetGeometryInstancedScene(
                        geometry, fbMesh->handle());
                }
                if (m.flags & Mod::flag_transform)
                {
                    std::memcpy(
                        _instances[m.index].affine, m.affine, sizeof(m.affine));
                }
                if (m.flags & Mod::flag_visibility)
                {
                    _instances[m.index].visible = true;
                }
                else
                {
                    _instances[m.index].visible = false;
                }
                _instances[m.index].dirty = true;
            }
            for (auto&& instance: _instances)
            {
                if (instance.dirty)
                {
                    auto geometry = instance.geometry;
                    rtcSetGeometryTransform(geometry, 0u, RTC_FORMAT_FLOAT3X4_ROW_MAJOR, instance.affine);
                    instance.visible ? rtcEnableGeometry(geometry) : rtcDisableGeometry(geometry);
                    rtcCommitGeometry(geometry);
                    instance.dirty = false;
                }
            }
            rtcCommitScene(_handle);
            auto error = rtcGetDeviceError(_device);
            if (error != RTC_ERROR_NONE)
            {
                printf("Embree Error: %d\n", error);
            }
        });
    }

    std::array<float, 12> FallbackAccel::_compress(float4x4 m) noexcept
    {
        return {
            m[0].x, m[1].x, m[2].x, m[3].x,
            m[0].y, m[1].y, m[2].y, m[3].y,
            m[0].z, m[1].z, m[2].z, m[3].z
        };
    }

    float4x4 FallbackAccel::_decompress(std::array<float, 12> m) noexcept
    {
        return luisa::make_float4x4(
            m[0], m[4], m[8], 0.f,
            m[1], m[5], m[9], 0.f,
            m[2], m[6], m[10], 0.f,
            m[3], m[7], m[11], 1.f);
    }

    namespace detail
    {
        void accel_trace_closest(const FallbackAccel* accel, float ox, float oy, float oz, float dx, float dy, float dz,
                                 float tmin, float tmax, uint mask, SurfaceHit* hit) noexcept
        {
#if LUISA_COMPUTE_EMBREE_VERSION == 3
            RTCIntersectContext ctx{};
            rtcInitIntersectContext(&ctx);
#else
    RTCRayQueryContext ctx{};
    rtcInitRayQueryContext(&ctx);
    RTCIntersectArguments args{.context = &ctx};
#endif
            RTCRayHit rh{};
            rh.ray.org_x = ox;
            rh.ray.org_y = oy;
            rh.ray.org_z = oz;
            rh.ray.dir_x = dx;
            rh.ray.dir_y = dy;
            rh.ray.dir_z = dz;
            rh.ray.tnear = tmin;
            rh.ray.tfar = tmax;

            rh.ray.mask = mask;
            rh.hit.geomID = RTC_INVALID_GEOMETRY_ID;
            rh.hit.primID = RTC_INVALID_GEOMETRY_ID;
            rh.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;
            rh.ray.flags = 0;
#if LUISA_COMPUTE_EMBREE_VERSION == 3
            rtcIntersect1(accel->scene(), &ctx, &rh);
#else
    rtcIntersect1(accel->scene(), &rh, &args);
#endif
            hit->inst = rh.hit.instID[0];
            hit->prim = rh.hit.primID;
            hit->bary = make_float2(rh.hit.u, rh.hit.v);
            hit->committed_ray_t = rh.ray.tfar;
        }
        void fill_transform(const FallbackAccel* accel, uint id, float4x4* buffer)
        {
            // TODO: handle embree 4

            // Retrieve the RTCInstance (you may need to store instances in your application)
            auto instance = rtcGetGeometry(accel->scene(), id);

            // Get the transform of the instance (a 4x4 matrix)
            rtcGetGeometryTransform(instance, 0.f, RTCFormat::RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR, buffer);
        }

        bool accel_trace_any(const FallbackAccel* accel, float ox, float oy, float oz, float dx, float dy, float dz,
                             float tmin, float tmax, uint mask) noexcept
        {
#if LUISA_COMPUTE_EMBREE_VERSION == 3
            RTCIntersectContext ctx{};
            rtcInitIntersectContext(&ctx);
#else
    RTCRayQueryContext ctx{};
    rtcInitRayQueryContext(&ctx);
    RTCOccludedArguments args{.context = &ctx};
#endif
            RTCRay ray{};
            ray.org_x = ox;
            ray.org_y = oy;
            ray.org_z = oz;
            ray.dir_x = dx;
            ray.dir_y = dy;
            ray.dir_z = dz;
            ray.tnear = tmin;
            ray.tfar = tmax;

            ray.mask = mask;
            ray.flags = 0;
#if LUISA_COMPUTE_EMBREE_VERSION == 3
            rtcOccluded1(accel->scene(), &ctx, &ray);
#else
    rtcOccluded1(accel->scene(), &ray, &args);
#endif
            return ray.tfar < 0.f;
        }
    } // namespace detail
} // namespace luisa::compute::fallback

void intersect_closest_wrapper(void* accel, float ox, float oy, float oz, float dx, float dy, float dz, float tmin,
                               float tmax, unsigned mask, void* hit)
{
    luisa::compute::fallback::detail::accel_trace_closest(
        reinterpret_cast<luisa::compute::fallback::FallbackAccel *>(accel),
        ox, oy, oz, dx, dy, dz, tmin, tmax, mask, reinterpret_cast<luisa::compute::SurfaceHit *>(hit));
}

void accel_transform_wrapper(void* accel, unsigned id, void* buffer)
{
    luisa::compute::fallback::detail::fill_transform(
        reinterpret_cast<luisa::compute::fallback::FallbackAccel *>(accel),
        id, reinterpret_cast<luisa::float4x4 *>(buffer));
    auto g = reinterpret_cast<luisa::float4x4 *>(buffer);
}
