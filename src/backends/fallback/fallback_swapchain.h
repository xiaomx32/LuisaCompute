//
// Created by swfly on 2024/12/2.
//

#pragma once
#include <luisa/runtime/swapchain.h>
//an extremely thin wrapper of CPU swapchain APIs

namespace luisa::compute::fallback
{
	class FallbackSwapchain {
	public:
		FallbackSwapchain(const luisa::compute::SwapchainOption &option);

		~FallbackSwapchain();

		void Present(void* image_data);
	protected:
		void* _swapchain_handle;
		luisa::uint2 size;

	};

}