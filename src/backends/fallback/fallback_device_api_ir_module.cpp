//
// Created by Mike on 2024/12/10.
//

#pragma once

#include <luisa/core/intrin.h>
#include "fallback_device_api_ir_module.h"

#if defined(LUISA_PLATFORM_WINDOWS)

#ifdef LUISA_ARCH_X86_64
#include "fallback_builtin/fallback_device_api_wrappers.windows.amd64.inl"
#else
#error Unsupported architecture
#endif

#elif defined(LUISA_PLATFORM_APPLE)

#error Not implemented

#else// Linux or other Unix-like systems

#error Not implemented

#endif

namespace luisa::compute::fallback {
luisa::string_view fallback_backend_device_builtin_module() noexcept {
    return luisa_fallback_backend_device_builtin_module;
}
}// namespace luisa::compute::fallback
