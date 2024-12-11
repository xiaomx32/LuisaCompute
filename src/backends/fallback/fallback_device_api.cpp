#include "fallback_texture.h"
#include "fallback_texture_bc.h"
#include "fallback_device_api.h"

namespace luisa::compute::fallback::api {

void luisa_bc7_read(const FallbackTextureView* tex, uint x, uint y, float4& out) noexcept {
    auto block_pos = make_uint2(x / 4, y / 4);
    auto block_per_row = tex->size2d().x / 4;
    const bc::BC7Block* bc_block = reinterpret_cast<const bc::BC7Block *>(tex->data()) +
                                   (block_pos.x + block_pos.y * block_per_row);
    bc_block->Decode(x % 4, y % 4, reinterpret_cast<float *>(&out));
}

void luisa_bc6h_read(const FallbackTextureView* tex, int x, int y, float4& out) noexcept {
    auto block_pos = make_uint2(x / 4, y / 4);
    auto block_per_row = tex->size2d().x / 4;
    const bc::BC6HBlock* bc_block = reinterpret_cast<const bc::BC6HBlock *>(tex->data()) + (
                                        block_pos.x + block_pos.y * block_per_row);
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
        default:{
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


template<typename T>
[[nodiscard]] inline auto texture_coord_point(Sampler::Address address, const T& uv, T s) noexcept
{
    switch (address)
    {
        case Sampler::Address::EDGE: return luisa::clamp(uv, 0.0f, one_minus_epsilon) * s;
        case Sampler::Address::REPEAT: return luisa::fract(uv) * s;
        case Sampler::Address::MIRROR:
        {
            auto uv0 = luisa::fmod(luisa::abs(uv), T{2.0f});
            uv0 = select(2.f - uv, uv, uv < T{1.f});
            return luisa::min(uv, one_minus_epsilon) * s;
        }
        case Sampler::Address::ZERO: return luisa::select(uv * s, T{65536.f}, uv < 0.f || uv >= 1.f);
    }
    return T{65536.f};
}
[[nodiscard]] inline auto texture_sample_point(FallbackTextureView* view, Sampler::Address address,
                                                   float u, float v) noexcept
{
    auto size = make_float2(view->size2d());
    auto p = texture_coord_point(address, make_float2(u,v), size);
    auto c = make_uint2(p);
    return texture_read_2d_float(view, c.x, c.y);
}

[[nodiscard]] inline auto texture_coord_linear(Sampler::Address address, float u, float v, float size_x, float size_y) noexcept
{
    auto s = make_float2(size_x, size_y);
    auto inv_s = 1.f / s;
    auto c_min = texture_coord_point(address, make_float2(u,v) - .5f * inv_s, s);
    auto c_max = texture_coord_point(address, make_float2(u,v) + .5f * inv_s, s);
    return std::make_pair(luisa::min(c_min, c_max), luisa::max(c_min, c_max));
}

[[nodiscard]] inline auto texture_sample_linear(FallbackTextureView* view, Sampler::Address address,
                                                    float u, float v) noexcept
{
    auto size = make_float2(view->size2d());
    auto [st_min, st_max] = texture_coord_linear(address, u,v, size.x, size.y);
    auto t = luisa::fract(st_max);
    auto c0 = make_uint2(st_min);
    auto c1 = make_uint2(st_max);
    auto v00 = texture_read_2d_float(view, c0.x, c0.y);
    auto v01 = texture_read_2d_float(view, c1.x, c0.y);
    auto v10 = texture_read_2d_float(view, c0.x, c1.y);
    auto v11 = texture_read_2d_float(view, c1.x, c1.y);
    return luisa::lerp(luisa::lerp(v00, v01, t.x),
                       luisa::lerp(v10, v11, t.x), t.y);
}

[[nodiscard]] float4 luisa_fallback_bindless_texture2d_sample(const Texture *handle, uint sampler, float u, float v) noexcept
{
    auto tex = reinterpret_cast<const FallbackTexture *>(&handle);
    auto s = Sampler::decode(sampler);
    auto view = tex->view(0);
    return s.filter() == Sampler::Filter::POINT
               ? bit_cast<float4>(texture_sample_point(&view, s.address(), u,v))
               : bit_cast<float4>(texture_sample_linear(&view, s.address(), u,v));
    return {};
}
[[nodiscard]] float4 luisa_fallback_bindless_texture2d_sample_level(const Texture *handle, uint sampler, float u, float v, float level) noexcept
{
    auto tex = reinterpret_cast<const FallbackTexture *>(&handle);
    auto s = Sampler::decode(sampler);
    auto filter = s.filter();
    if (level <= 0.f || tex->mip_levels() == 0u ||
        filter == Sampler::Filter::POINT)
    {
        return bit_cast<float4>(bindless_texture_2d_sample(tex, sampler, u, v));
    }
    auto level0 = std::min(static_cast<uint32_t>(level),
                           tex->mip_levels() - 1u);
    auto view0 = tex->view(level0);
    auto v0 = texture_sample_linear(
        &view0, s.address(), u, v);
    if (level0 == tex->mip_levels() - 1u ||
        filter == Sampler::Filter::LINEAR_POINT)
    {
        return bit_cast<float4>(v0);
    }
    auto view1 = tex->view(level0 + 1);
    auto v1 = texture_sample_linear(
        &view1, s.address(), u, v);
    return bit_cast<float4>(luisa::lerp(v0, v1, luisa::fract(level)));
}
[[nodiscard]] float4 luisa_fallback_bindless_texture2d_sample_grad(const Texture *handle, uint sampler, float u, float v, float dudx, float dudy, float dvdx, float dvdy) noexcept { return {}; }
[[nodiscard]] float4 luisa_fallback_bindless_texture2d_sample_grad_level(const Texture *handle, uint sampler, float u, float v, float dudx, float dudy, float dvdx, float dvdy, float level) noexcept { return {}; }

[[nodiscard]] float4 luisa_fallback_bindless_texture3d_sample(const Texture *handle, uint sampler, float u, float v, float w) noexcept { return {}; }
[[nodiscard]] float4 luisa_fallback_bindless_texture3d_sample_level(const Texture *handle, uint sampler, float u, float v, float w, float level) noexcept { return {}; }
[[nodiscard]] float4 luisa_fallback_bindless_texture3d_sample_grad(const Texture *handle, uint sampler, float u, float v, float w, float dudx, float dvdx, float dwdx, float dudy, float dvdy, float dwdy) noexcept { return {}; }
[[nodiscard]] float4 luisa_fallback_bindless_texture3d_sample_grad_level(const Texture *handle, uint sampler, float u, float v, float w, float dudx, float dvdx, float dwdx, float dudy, float dvdy, float dwdy, float level) noexcept { return {}; }

}// namespace luisa::compute::fallback::api
