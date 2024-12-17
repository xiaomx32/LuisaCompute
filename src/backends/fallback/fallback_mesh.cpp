//
// Created by Mike Smith on 2022/2/11.
//

#include <luisa/core/logging.h>

#include "fallback_buffer.h"
#include "fallback_mesh.h"

namespace luisa::compute::fallback {

FallbackMesh::FallbackMesh(RTCDevice device, const AccelOption &option) noexcept
    : _handle{rtcNewScene(device)},
      _geometry{rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE)} {
    luisa_fallback_accel_set_flags(_handle, option);
    rtcAttachGeometry(_handle, _geometry);
    rtcReleaseGeometry(_geometry);// already moved into the scene
}

FallbackMesh::~FallbackMesh() noexcept {
    rtcReleaseScene(_handle);
}

void FallbackMesh::build(luisa::unique_ptr<MeshBuildCommand> cmd) noexcept {
    auto v_buffer = reinterpret_cast<FallbackBuffer *>(cmd->vertex_buffer())->data();
    auto t_buffer = reinterpret_cast<FallbackBuffer *>(cmd->triangle_buffer())->data();
    LUISA_DEBUG_ASSERT(cmd->vertex_buffer_size() % cmd->vertex_stride() == 0u, "Invalid vertex buffer size.");
    LUISA_DEBUG_ASSERT(cmd->triangle_buffer_size() % sizeof(Triangle) == 0u, "Invalid triangle buffer size.");
    auto v_count = cmd->vertex_buffer_size() / cmd->vertex_stride();
    auto t_count = cmd->triangle_buffer_size() / sizeof(Triangle);
    rtcSetSharedGeometryBuffer(_geometry, RTC_BUFFER_TYPE_VERTEX, 0u, RTC_FORMAT_FLOAT3,
                               v_buffer, cmd->vertex_buffer_offset(), cmd->vertex_stride(), v_count);
    rtcSetSharedGeometryBuffer(_geometry, RTC_BUFFER_TYPE_INDEX, 0u, RTC_FORMAT_UINT3,
                               t_buffer, cmd->triangle_buffer_offset(), sizeof(Triangle), t_count);
    rtcCommitGeometry(_geometry);
    rtcCommitScene(_handle);
}

}// namespace luisa::compute::fallback
