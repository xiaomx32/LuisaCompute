#pragma once
#include <luisa/backends/ext/dx_hdr_ext_interface.h>
#include <luisa/runtime/swapchain.h>
#include <luisa/runtime/stream.h>
namespace luisa::compute {
Swapchain DXHDRExt::create_swapchain(const Stream &stream, const DXSwapchainOption &option) noexcept {
    return Swapchain{stream.device(), create_swapchain(option, stream.handle())};
}
}// namespace luisa::compute