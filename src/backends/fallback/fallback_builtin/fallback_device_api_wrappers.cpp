#define LUISA_COMPUTE_FALLBACK_DEVICE_LIB
#include "../fallback_device_api.h"

extern "C" {// wrappers

#define LUISA_FALLBACK_WRAPPER __attribute__((visibility("hidden"))) __attribute__((used))

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_texture2d_read_int(const TextureView *handle, const uint2 *coord, int4 *out) noexcept {
    *out = luisa_fallback_texture2d_read_int(*handle, coord->x, coord->y);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_texture2d_read_uint(const TextureView *handle, const uint2 *coord, uint4 *out) noexcept {
    *out = luisa_fallback_texture2d_read_uint(*handle, coord->x, coord->y);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_texture2d_read_float(const TextureView *handle, const uint2 *coord, float4 *out) noexcept {
    *out = luisa_fallback_texture2d_read_float(*handle, coord->x, coord->y);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_texture2d_write_float(const TextureView *handle, const uint2 *coord, const float4 *value) noexcept {
    luisa_fallback_texture2d_write_float(*handle, coord->x, coord->y, *value);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_texture2d_write_uint(const TextureView *handle, const uint2 *coord, const uint4 *value) noexcept {
    luisa_fallback_texture2d_write_uint(*handle, coord->x, coord->y, *value);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_texture2d_write_int(const TextureView *handle, const uint2 *coord, const int4 *value) noexcept {
    luisa_fallback_texture2d_write_int(*handle, coord->x, coord->y, *value);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_texture3d_read_int(const TextureView *handle, const uint3 *coord, int4 *out) noexcept {
    *out = luisa_fallback_texture3d_read_int(*handle, coord->x, coord->y, coord->z);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_texture3d_read_uint(const TextureView *handle, const uint3 *coord, uint4 *out) noexcept {
    *out = luisa_fallback_texture3d_read_uint(*handle, coord->x, coord->y, coord->z);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_texture3d_read_float(const TextureView *handle, const uint3 *coord, float4 *out) noexcept {
    *out = luisa_fallback_texture3d_read_float(*handle, coord->x, coord->y, coord->z);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_texture3d_write_float(const TextureView *handle, const uint3 *coord, const float4 *value) noexcept {
    luisa_fallback_texture3d_write_float(*handle, coord->x, coord->y, coord->z, *value);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_texture3d_write_uint(const TextureView *handle, const uint3 *coord, const uint4 *value) noexcept {
    luisa_fallback_texture3d_write_uint(*handle, coord->x, coord->y, coord->z, *value);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_texture3d_write_int(const TextureView *handle, const uint3 *coord, const int4 *value) noexcept {
    luisa_fallback_texture3d_write_int(*handle, coord->x, coord->y, coord->z, *value);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_bindless_texture2d_sample(const Texture *handle, uint sampler, const float2 *uv, float4 *out) noexcept {
    *out = luisa_fallback_bindless_texture2d_sample(handle, sampler, uv->x, uv->y);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_bindless_texture2d_sample_level(const Texture *handle, uint sampler, const float2 *uv, float level, float4 *out) noexcept {
    *out = luisa_fallback_bindless_texture2d_sample_level(handle, sampler, uv->x, uv->y, level);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_bindless_texture2d_sample_grad(const Texture *handle, uint sampler, const float2 *uv, const float2 *ddx, const float2 *ddy, float4 *out) noexcept {
    *out = luisa_fallback_bindless_texture2d_sample_grad(handle, sampler, uv->x, uv->y, ddx->x, ddx->y, ddy->x, ddy->y);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_bindless_texture2d_sample_grad_level(const Texture *handle, uint sampler, const float2 *uv, const float2 *ddx, const float2 *ddy, float level, float4 *out) noexcept {
    *out = luisa_fallback_bindless_texture2d_sample_grad_level(handle, sampler, uv->x, uv->y, ddx->x, ddx->y, ddy->x, ddy->y, level);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_bindless_texture3d_sample(const Texture *handle, uint sampler, const float3 *uvw, float4 *out) noexcept {
    *out = luisa_fallback_bindless_texture3d_sample(handle, sampler, uvw->x, uvw->y, uvw->z);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_bindless_texture3d_sample_level(const Texture *handle, uint sampler, const float3 *uvw, float level, float4 *out) noexcept {
    *out = luisa_fallback_bindless_texture3d_sample_level(handle, sampler, uvw->x, uvw->y, uvw->z, level);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_bindless_texture3d_sample_grad(const Texture *handle, uint sampler, const float3 *uvw, const float3 *ddx, const float3 *ddy, float4 *out) noexcept {
    *out = luisa_fallback_bindless_texture3d_sample_grad(handle, sampler, uvw->x, uvw->y, uvw->z, ddx->x, ddx->y, ddx->z, ddy->x, ddy->y, ddy->z);
}

LUISA_FALLBACK_WRAPPER void luisa_fallback_wrapper_bindless_texture3d_sample_grad_level(const Texture *handle, uint sampler, const float3 *uvw, const float3 *ddx, const float3 *ddy, float level, float4 *out) noexcept {
    *out = luisa_fallback_bindless_texture3d_sample_grad_level(handle, sampler, uvw->x, uvw->y, uvw->z, ddx->x, ddx->y, ddx->z, ddy->x, ddy->y, ddy->z, level);
}

}
