#pragma once

#ifndef LUISA_COMPUTE_FALLBACK_DEVICE_LIB
namespace luisa::compute::fallback::api {
#endif

extern "C" {

using uint = unsigned int;

struct alignas(8) float2 {
    float x, y;
};

struct alignas(16) float3 {
    float x, y, z;
};

struct alignas(16) float4 {
    float x, y, z, w;
};

struct alignas(16) uint4 {
    uint x, y, z, w;
};

struct alignas(16) int4 {
    int x, y, z, w;
};

struct alignas(8) uint2 {
    uint x, y;
};

struct alignas(16) uint3 {
    uint x, y, z;
};

struct alignas(16) Ray {
    float origin[3];
    float t_min;
    float direction[3];
    float t_max;
};

struct SurfaceHit {
    uint inst;
    uint prim;
    float2 bary;
    float committed_ray_t;
};

struct alignas(16u) TextureView {
    void *_data;
    uint _width : 16u;
    uint _height : 16u;
    uint _depth : 16u;
    uint _storage : 8u;
    uint _dimension : 4u;
    uint _pixel_stride_shift : 4u;
};

/* implementations */

struct Texture;

[[nodiscard]] int4 luisa_fallback_texture2d_read_int(TextureView handle, uint x, uint y) noexcept;
[[nodiscard]] uint4 luisa_fallback_texture2d_read_uint(TextureView handle, uint x, uint y) noexcept;
[[nodiscard]] float4 luisa_fallback_texture2d_read_float(TextureView handle, uint x, uint y) noexcept;

void luisa_fallback_texture2d_write_float(TextureView handle, uint x, uint y, float4 value) noexcept;
void luisa_fallback_texture2d_write_uint(TextureView handle, uint x, uint y, uint4 value) noexcept;
void luisa_fallback_texture2d_write_int(TextureView handle, uint x, uint y, int4 value) noexcept;

[[nodiscard]] int4 luisa_fallback_texture3d_read_int(TextureView handle, uint x, uint y, uint z) noexcept;
[[nodiscard]] uint4 luisa_fallback_texture3d_read_uint(TextureView handle, uint x, uint y, uint z) noexcept;
[[nodiscard]] float4 luisa_fallback_texture3d_read_float(TextureView handle, uint x, uint y, uint z) noexcept;

void luisa_fallback_texture3d_write_float(TextureView handle, uint x, uint y, uint z, float4 value) noexcept;
void luisa_fallback_texture3d_write_uint(TextureView handle, uint x, uint y, uint z, uint4 value) noexcept;
void luisa_fallback_texture3d_write_int(TextureView handle, uint x, uint y, uint z, int4 value) noexcept;

[[nodiscard]] float4 luisa_fallback_bindless_texture2d_sample(const Texture *handle, uint sampler, float u, float v) noexcept;
[[nodiscard]] float4 luisa_fallback_bindless_texture2d_sample_level(const Texture *handle, uint sampler, float u, float v, float level) noexcept;
[[nodiscard]] float4 luisa_fallback_bindless_texture2d_sample_grad(const Texture *handle, uint sampler, float u, float v, float dudx, float dudy, float dvdx, float dvdy) noexcept;
[[nodiscard]] float4 luisa_fallback_bindless_texture2d_sample_grad_level(const Texture *handle, uint sampler, float u, float v, float dudx, float dudy, float dvdx, float dvdy, float level) noexcept;

[[nodiscard]] float4 luisa_fallback_bindless_texture3d_sample(const Texture *handle, uint sampler, float u, float v, float w) noexcept;
[[nodiscard]] float4 luisa_fallback_bindless_texture3d_sample_level(const Texture *handle, uint sampler, float u, float v, float w, float level) noexcept;
[[nodiscard]] float4 luisa_fallback_bindless_texture3d_sample_grad(const Texture *handle, uint sampler, float u, float v, float w, float dudx, float dvdx, float dwdx, float dudy, float dvdy, float dwdy) noexcept;
[[nodiscard]] float4 luisa_fallback_bindless_texture3d_sample_grad_level(const Texture *handle, uint sampler, float u, float v, float w, float dudx, float dvdx, float dwdx, float dudy, float dvdy, float dwdy, float level) noexcept;

}

#ifndef LUISA_COMPUTE_FALLBACK_DEVICE_LIB
}// namespace luisa::compute::fallback
#endif
