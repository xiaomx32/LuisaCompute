//
// Created by swfly on 2024/12/2.
//

#pragma once

#include <luisa/runtime/swapchain.h>

namespace luisa::compute::fallback {

class FallbackTexture;
class FallbackStream;

class FallbackSwapchain {
private:
    void *_handle;
    luisa::uint2 size;

public:
    explicit FallbackSwapchain(const luisa::compute::SwapchainOption &option);
    ~FallbackSwapchain() noexcept;
    void present(FallbackStream *stream, FallbackTexture *frame);
};

}// namespace luisa::compute::fallback