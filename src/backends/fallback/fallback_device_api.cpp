#include "fallback_texture.h"
#include "fallback_texture_bc.h"
#include "fallback_accel.h"
#include "fallback_device_api.h"

namespace luisa::compute::fallback::api {

void luisa_bc7_read(const FallbackTextureView *tex, uint x, uint y, float4 &out) noexcept {
    auto block_pos = make_uint2(x / 4, y / 4);
    auto block_per_row = tex->size2d().x / 4;
    const bc::BC7Block *bc_block = reinterpret_cast<const bc::BC7Block *>(tex->data()) +
                                   (block_pos.x + block_pos.y * block_per_row);
    bc_block->Decode(x % 4, y % 4, reinterpret_cast<float *>(&out));
}

void luisa_bc6h_read(const FallbackTextureView *tex, int x, int y, float4 &out) noexcept {
    auto block_pos = make_uint2(x / 4, y / 4);
    auto block_per_row = tex->size2d().x / 4;
    const bc::BC6HBlock *bc_block = reinterpret_cast<const bc::BC6HBlock *>(tex->data()) + (block_pos.x + block_pos.y * block_per_row);
    bc_block->Decode(false, x % 4, y % 4, reinterpret_cast<float *>(&out));
}

[[nodiscard]] int4 luisa_fallback_texture2d_read_int(TextureView handle, uint x, uint y) noexcept {
    auto tex = reinterpret_cast<const FallbackTextureView *>(&handle);
    switch (tex->storage()) {
        case PixelStorage::BC7: {
            float4 out;
            luisa_bc7_read(tex, x, y, out);
            return {static_cast<int>(out.x * 255.f),
                    static_cast<int>(out.y * 255.f),
                    static_cast<int>(out.z * 255.f),
                    static_cast<int>(out.w * 255.f)};
        }
        case PixelStorage::BC6: {
            float4 out;
            luisa_bc6h_read(tex, x, y, out);
            return {static_cast<int>(out.x * 255.f),
                    static_cast<int>(out.y * 255.f),
                    static_cast<int>(out.z * 255.f),
                    static_cast<int>(out.w * 255.f)};
        }
        default: {
            auto v = tex->read2d<int>(make_uint2(x, y));
            return {v.x, v.y, v.z, v.w};
        }
    }
}

[[nodiscard]] uint4 luisa_fallback_texture2d_read_uint(TextureView handle, uint x, uint y) noexcept {
    auto tex = reinterpret_cast<const FallbackTextureView *>(&handle);
    switch (tex->storage()) {
        case PixelStorage::BC7: {
            float4 out;
            luisa_bc7_read(tex, x, y, out);
            return {static_cast<uint>(out.x * 255.f),
                    static_cast<uint>(out.y * 255.f),
                    static_cast<uint>(out.z * 255.f),
                    static_cast<uint>(out.w * 255.f)};
        }
        case PixelStorage::BC6: {
            float4 out;
            luisa_bc6h_read(tex, x, y, out);
            return {static_cast<uint>(out.x * 255.f),
                    static_cast<uint>(out.y * 255.f),
                    static_cast<uint>(out.z * 255.f),
                    static_cast<uint>(out.w * 255.f)};
        }
        default: {
            auto v = tex->read2d<uint>(make_uint2(x, y));
            return {v.x, v.y, v.z, v.w};
        }
    }
}

[[nodiscard]] float4 luisa_fallback_texture2d_read_float(TextureView handle, uint x, uint y) noexcept {
    auto tex = reinterpret_cast<const FallbackTextureView *>(&handle);
    switch (tex->storage()) {
        case PixelStorage::BC7: {
            float4 out;
            luisa_bc7_read(tex, x, y, out);
            return out;
        }
        case PixelStorage::BC6: {
            float4 out;
            luisa_bc6h_read(tex, x, y, out);
            return out;
        }
        default: {
            auto v = tex->read2d<float>(make_uint2(x, y));
            return {v.x, v.y, v.z, v.w};
        }
    }
}

void luisa_fallback_texture2d_write_float(TextureView handle, uint x, uint y, float4 value) noexcept {
    auto tex = reinterpret_cast<const FallbackTextureView *>(&handle);
    switch (tex->storage()) {
        case PixelStorage::BC7: {
            LUISA_ERROR("cannot write to BC texture");
            break;
        }
        case PixelStorage::BC6: {
            LUISA_ERROR("cannot write to BC texture");
            break;
        }
        default: {
            auto v = luisa::make_float4(value.x, value.y, value.z, value.w);
            tex->write2d<float>(make_uint2(x, y), v);
        }
    }
}

void luisa_fallback_texture2d_write_uint(TextureView handle, uint x, uint y, uint4 value) noexcept {
    auto tex = reinterpret_cast<const FallbackTextureView *>(&handle);
    switch (tex->storage()) {
        case PixelStorage::BC7: {
            LUISA_ERROR("cannot write to BC texture");
            break;
        }
        case PixelStorage::BC6: {
            LUISA_ERROR("cannot write to BC texture");
            break;
        }
        default: {
            auto v = luisa::make_uint4(value.x, value.y, value.z, value.w);
            tex->write2d<uint>(make_uint2(x, y), v);
        }
    }
}

void luisa_fallback_texture2d_write_int(TextureView handle, uint x, uint y, int4 value) noexcept {
    auto tex = reinterpret_cast<const FallbackTextureView *>(&handle);
    switch (tex->storage()) {
        case PixelStorage::BC7: {
            LUISA_ERROR("cannot write to BC texture");
            break;
        }
        case PixelStorage::BC6: {
            LUISA_ERROR("cannot write to BC texture");
            break;
        }
        default: {
            auto v = luisa::make_int4(value.x, value.y, value.z, value.w);
            tex->write2d<int>(make_uint2(x, y), v);
        }
    }
}

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

[[nodiscard]] float4 luisa_fallback_bindless_texture2d_read(const Texture *handle, uint x, uint y) noexcept {
    return {};
}

[[nodiscard]] float4 luisa_fallback_bindless_texture2d_read_level(const Texture *handle, uint x, uint y, uint level) noexcept {
    return {};
}

[[nodiscard]] float4 luisa_fallback_bindless_texture3d_read(const Texture *handle, uint x, uint y, uint z) noexcept {
    return {};
}

[[nodiscard]] float4 luisa_fallback_bindless_texture3d_read_level(const Texture *handle, uint x, uint y, uint z, uint level) noexcept {
    return {};
}

[[nodiscard]] SurfaceHit luisa_fallback_accel_trace_closest(const Accel *handle, float ox, float oy, float oz, float t_min, float dx, float dy, float dz, float t_max, uint mask, float time) noexcept {
#if LUISA_COMPUTE_EMBREE_VERSION == 3
    RTCIntersectContext ctx{};
    rtcInitIntersectContext(&ctx);
#else
    RTCRayQueryContext ctx{};
    rtcInitRayQueryContext(&ctx);
    RTCIntersectArguments args{.context = &ctx};
#endif
    RTCRayHit rh{};
    rh.ray.org_x = ox;
    rh.ray.org_y = oy;
    rh.ray.org_z = oz;
    rh.ray.dir_x = dx;
    rh.ray.dir_y = dy;
    rh.ray.dir_z = dz;
    rh.ray.tnear = t_min;
    rh.ray.tfar = t_max;
    rh.ray.mask = mask;
    rh.ray.time = time;
    rh.ray.flags = 0;

    rh.hit.geomID = RTC_INVALID_GEOMETRY_ID;
    rh.hit.primID = RTC_INVALID_GEOMETRY_ID;
    rh.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;
    auto accel = reinterpret_cast<const FallbackAccel *>(handle);
#if LUISA_COMPUTE_EMBREE_VERSION == 3
    rtcIntersect1(accel->scene(), &ctx, &rh);
#else
    rtcIntersect1(accel->scene(), &rh, &args);
#endif
    SurfaceHit hit{};
    hit.inst = rh.hit.instID[0];
    hit.prim = rh.hit.primID;
    hit.bary = {rh.hit.u, rh.hit.v};
    hit.committed_ray_t = rh.ray.tfar;
    return hit;
}

[[nodiscard]] bool luisa_fallback_accel_trace_any(const Accel *handle, float ox, float oy, float oz, float t_min, float dx, float dy, float dz, float t_max, uint mask, float time) noexcept {

#if LUISA_COMPUTE_EMBREE_VERSION == 3
    RTCIntersectContext ctx{};
    rtcInitIntersectContext(&ctx);
#else
    RTCRayQueryContext ctx{};
    rtcInitRayQueryContext(&ctx);
    RTCOccludedArguments args{.context = &ctx};
#endif
    RTCRay ray{};
    ray.org_x = ox;
    ray.org_y = oy;
    ray.org_z = oz;
    ray.dir_x = dx;
    ray.dir_y = dy;
    ray.dir_z = dz;
    ray.tnear = t_min;
    ray.tfar = t_max;
    ray.mask = mask;
    ray.time = time;
    ray.flags = 0;

    auto accel = reinterpret_cast<const FallbackAccel *>(handle);
#if LUISA_COMPUTE_EMBREE_VERSION == 3
    rtcOccluded1(accel->scene(), &ctx, &ray);
#else
    rtcOccluded1(accel->scene(), &ray, &args);
#endif
    return ray.tfar < 0.f;
}

}// namespace luisa::compute::fallback::api
