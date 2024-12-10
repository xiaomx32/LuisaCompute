#include "fallback_device_api.h"

namespace luisa::compute::fallback::api {

[[nodiscard]] int4 luisa_fallback_texture2d_read_int(TextureView handle, uint x, uint y) noexcept { return {}; }
[[nodiscard]] uint4 luisa_fallback_texture2d_read_uint(TextureView handle, uint x, uint y) noexcept { return {}; }
[[nodiscard]] float4 luisa_fallback_texture2d_read_float(TextureView handle, uint x, uint y) noexcept { return {}; }

void luisa_fallback_texture2d_write_float(TextureView handle, uint x, uint y, float4 value) noexcept {}
void luisa_fallback_texture2d_write_uint(TextureView handle, uint x, uint y, uint4 value) noexcept {}
void luisa_fallback_texture2d_write_int(TextureView handle, uint x, uint y, int4 value) noexcept {}

[[nodiscard]] int4 luisa_fallback_texture3d_read_int(TextureView handle, uint x, uint y, uint z) noexcept { return {}; }
[[nodiscard]] uint4 luisa_fallback_texture3d_read_uint(TextureView handle, uint x, uint y, uint z) noexcept { return {}; }
[[nodiscard]] float4 luisa_fallback_texture3d_read_float(TextureView handle, uint x, uint y, uint z) noexcept { return {}; }

void luisa_fallback_texture3d_write_float(TextureView handle, uint x, uint y, uint z, float4 value) noexcept {}
void luisa_fallback_texture3d_write_uint(TextureView handle, uint x, uint y, uint z, uint4 value) noexcept {}
void luisa_fallback_texture3d_write_int(TextureView handle, uint x, uint y, uint z, int4 value) noexcept {}

[[nodiscard]] float4 luisa_fallback_bindless_texture2d_sample(const Texture *handle, uint sampler, float u, float v) noexcept { return {}; }
[[nodiscard]] float4 luisa_fallback_bindless_texture2d_sample_level(const Texture *handle, uint sampler, float u, float v, float level) noexcept { return {}; }
[[nodiscard]] float4 luisa_fallback_bindless_texture2d_sample_grad(const Texture *handle, uint sampler, float u, float v, float dudx, float dudy, float dvdx, float dvdy) noexcept { return {}; }
[[nodiscard]] float4 luisa_fallback_bindless_texture2d_sample_grad_level(const Texture *handle, uint sampler, float u, float v, float dudx, float dudy, float dvdx, float dvdy, float level) noexcept { return {}; }

[[nodiscard]] float4 luisa_fallback_bindless_texture3d_sample(const Texture *handle, uint sampler, float u, float v, float w) noexcept { return {}; }
[[nodiscard]] float4 luisa_fallback_bindless_texture3d_sample_level(const Texture *handle, uint sampler, float u, float v, float w, float level) noexcept { return {}; }
[[nodiscard]] float4 luisa_fallback_bindless_texture3d_sample_grad(const Texture *handle, uint sampler, float u, float v, float w, float dudx, float dvdx, float dwdx, float dudy, float dvdy, float dwdy) noexcept { return {}; }
[[nodiscard]] float4 luisa_fallback_bindless_texture3d_sample_grad_level(const Texture *handle, uint sampler, float u, float v, float w, float dudx, float dvdx, float dwdx, float dudy, float dvdy, float dwdy, float level) noexcept { return {}; }

}// namespace luisa::compute::fallback::api
