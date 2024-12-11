//
// Created by Mike Smith on 2022/2/11.
//

#include <luisa/core/stl.h>
#include <luisa/core/logging.h>

#include "fallback_mesh.h"
#include "fallback_accel.h"
#include "thread_pool.h"

namespace luisa::compute::fallback {

FallbackAccel::FallbackAccel(RTCDevice device, AccelUsageHint hint) noexcept
    : _handle{rtcNewScene(device)}, _device(device) {
    switch (hint) {
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

void FallbackAccel::build(ThreadPool &pool, size_t instance_count,
                          luisa::vector<AccelBuildCommand::Modification> &&mods) noexcept {
    using Mod = AccelBuildCommand::Modification;
    pool.async([this, instance_count, mods = std::move(mods)]() {
        if (instance_count < _instances.size()) {
            // remove redundant geometries
            for (auto i = instance_count; i < _instances.size(); i++) { rtcDetachGeometry(_handle, i); }
            _instances.resize(instance_count);
        } else {
            // create new geometries
            auto device = rtcGetSceneDevice(_handle);
            _instances.reserve(next_pow2(instance_count));
            for (auto i = _instances.size(); i < instance_count; i++) {
                auto geometry = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_INSTANCE);
                rtcSetGeometryBuildQuality(geometry, RTC_BUILD_QUALITY_HIGH);
                rtcAttachGeometryByID(_handle, geometry, i);
                rtcReleaseGeometry(geometry);// already moved into the scene
                _instances.emplace_back().geometry = geometry;
            }
        }
        for (auto m : mods) {
            if (m.flags & Mod::flag_primitive) {
                auto geometry = _instances[m.index].geometry;
                auto mesh = reinterpret_cast<const FallbackMesh *>(m.primitive);
                rtcSetGeometryInstancedScene(geometry, mesh->handle());
            }
            if (m.flags & Mod::flag_transform) {
                std::memcpy(_instances[m.index].affine, m.affine, sizeof(m.affine));
            }
            if (m.flags & Mod::flag_visibility) {
                _instances[m.index].mask = m.vis_mask;
            }
            if (m.flags & Mod::flag_user_id) {
                _instances[m.index].user_id = m.user_id;
            }
            if (m.flags & Mod::flag_opaque) {
                _instances[m.index].opaque = m.flags & Mod::flag_opaque_on;
            }
            _instances[m.index].dirty = true;
        }
        for (auto &&instance : _instances) {
            if (instance.dirty) {
                auto geometry = instance.geometry;
                rtcSetGeometryTransform(geometry, 0u, RTC_FORMAT_FLOAT3X4_ROW_MAJOR, instance.affine);
                rtcSetGeometryMask(geometry, instance.mask);
                rtcCommitGeometry(geometry);
                instance.dirty = false;
            }
        }
        rtcCommitScene(_handle);
        auto error = rtcGetDeviceError(_device);
        if (error != RTC_ERROR_NONE) {
            LUISA_INFO("RTC ERROR: {}", (uint)error);
        }
    });
}

}// namespace luisa::compute::fallback
