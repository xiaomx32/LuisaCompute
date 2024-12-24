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

public:
    FallbackMesh(RTCDevice device, const AccelOption &option) noexcept;
    ~FallbackMesh() noexcept;
    [[nodiscard]] auto handle() const noexcept { return _handle; }
    void build(luisa::unique_ptr<MeshBuildCommand> cmd) noexcept;
};

}// namespace luisa::compute::llvm
