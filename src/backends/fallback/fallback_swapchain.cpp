//
// Created by swfly on 2024/12/2.
//

#include "fallback_swapchain.h"
#include "fallback_texture.h"
//swapchain
extern "C"
{
void *luisa_compute_create_cpu_swapchain(uint64_t display_handle, uint64_t window_handle,
		unsigned width, unsigned height, bool allow_hdr, bool vsync,
		unsigned back_buffer_count) noexcept;

uint8_t luisa_compute_cpu_swapchain_storage(void *swapchain) noexcept;

void *luisa_compute_cpu_swapchain_native_handle(void *swapchain) noexcept;

void luisa_compute_destroy_cpu_swapchain(void *swapchain) noexcept ;

void luisa_compute_cpu_swapchain_present(void *swapchain, const void *pixels, uint64_t size) noexcept ;

void luisa_compute_cpu_swapchain_present_with_callback(void *swapchain, void *ctx, void (*blit)(void *ctx, void *mapped_pixels)) noexcept;
}

luisa::compute::fallback::FallbackSwapchain::FallbackSwapchain(const luisa::compute::SwapchainOption& option)
{
	_swapchain_handle = luisa_compute_create_cpu_swapchain(option.display,option.window,option.size.x, option.size.y, option.wants_hdr, option.wants_vsync,
			option.back_buffer_count);
	size = option.size;
}

luisa::compute::fallback::FallbackSwapchain::~FallbackSwapchain()
{
	luisa_compute_destroy_cpu_swapchain(_swapchain_handle);
}

void luisa::compute::fallback::FallbackSwapchain::Present(void* image_data)
{
	std::vector<std::byte> bytes{};
	auto image = reinterpret_cast<FallbackTexture*>(image_data);
	luisa_compute_cpu_swapchain_present(_swapchain_handle, image->view(0).data(), size.x*size.y * 4);
}
