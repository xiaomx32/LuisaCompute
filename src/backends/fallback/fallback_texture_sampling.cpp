//
// Created by swfly on 2024/11/16.
//

#include "fallback_texture.h"
#include "fallback_texture_bc.h"
#include "llvm_abi.h"

//swfly tries to write more about sampling bc textures
namespace luisa::compute::fallback
{
    void texture_write_2d_float_wrapper(void* ptr, uint x, uint y, void* val)
    {
        texture_write_2d_float(reinterpret_cast<const FallbackTextureView *>(ptr), x, y, *(float4 *) val);
    }

    void texture_read_2d_float_wrapper(void* ptr, uint x, uint y, void* out)
    {
        *(float4 *) out = texture_read_2d_float(reinterpret_cast<const FallbackTextureView *>(ptr), x, y);
    }

    void texture_write_2d_uint_wrapper(void* ptr, uint x, uint y, void* val)
    {
        texture_write_2d_uint(reinterpret_cast<const FallbackTextureView *>(ptr), x, y, *(uint4 *) val);
    }

    void texture_read_2d_uint_wrapper(void* ptr, uint x, uint y, void* out)
    {
        *(uint4 *) out = texture_read_2d_uint(reinterpret_cast<const FallbackTextureView *>(ptr), x, y);
    }

    void texture_read_3d_float_wrapper(void* ptr, uint x, uint y, uint z, void* out)
    {
        *(float4 *) out = texture_read_3d_float(reinterpret_cast<const FallbackTextureView *>(ptr), x, y, z);
    }
    void texture_read_3d_uint_wrapper(void* ptr, uint x, uint y, uint z, void* out)
    {
        *(uint4 *) out = texture_read_3d_uint(reinterpret_cast<const FallbackTextureView *>(ptr), x, y, z);
    }







    void luisa_bc7_read(const FallbackTextureView* tex, uint x, uint y, float4& out) noexcept
    {
        auto block_pos = make_uint2(x / 4, y / 4);
        auto block_per_row = tex->size2d().x / 4;
        const bc::BC7Block* bc_block = reinterpret_cast<const bc::BC7Block *>(tex->data()) + (
                                           block_pos.x + block_pos.y * block_per_row);
        bc_block->Decode(x % 4, y % 4, reinterpret_cast<float *>(&out));
    }

    void luisa_bc6h_read(const FallbackTextureView* tex, int x, int y, float4& out) noexcept
    {
        auto block_pos = make_uint2(x / 4, y / 4);
        auto block_per_row = tex->size2d().x / 4;
        const bc::BC6HBlock* bc_block = reinterpret_cast<const bc::BC6HBlock *>(tex->data()) + (
                                            block_pos.x + block_pos.y * block_per_row);
        bc_block->Decode(false, x % 4, y % 4, reinterpret_cast<float *>(&out));
    }

    int4 fallback::texture_read_2d_int(const FallbackTextureView* tex, uint x, uint y) noexcept
    {
        switch (tex->storage())
        {
            case PixelStorage::BC7:
            {
                float4 out;
                luisa_bc7_read(tex, x, y, out);
                return make_int4(out.x * 255.f, out.y * 255.f, out.z * 255.f, out.w * 255.f);
            }
            case PixelStorage::BC6:
            {
                float4 out;
                luisa_bc6h_read(tex, x, y, out);
                return make_int4(out.x * 255.f, out.y * 255.f, out.z * 255.f, out.w * 255.f);
            }
            default:
                return tex->read2d<int>(make_uint2(x, y));
        }
    }

    int4 fallback::texture_read_3d_int(const FallbackTextureView* tex, uint x, uint y, uint z) noexcept
    {
        switch (tex->storage())
        {
            case PixelStorage::BC7:
            {
                LUISA_ERROR("Block compression doesn't work for 3D texture");
                return make_int4(0);
            }
            case PixelStorage::BC6:
            {
                LUISA_ERROR("Block compression doesn't work for 3D texture");
                return make_int4(0);
            }
            default:
                return tex->read2d<int>(make_uint2(x, y));
        }
    }

    uint4 fallback::texture_read_2d_uint(const FallbackTextureView* tex, uint x, uint y) noexcept
    {
        switch (tex->storage())
        {
            case PixelStorage::BC7:
            {
                float4 out;
                luisa_bc7_read(tex, x, y, out);
                return make_uint4(out.x * 255.f, out.y * 255.f, out.z * 255.f, out.w * 255.f);
            }
            case PixelStorage::BC6:
            {
                float4 out;
                luisa_bc6h_read(tex, x, y, out);
                return make_uint4(out.x * 255.f, out.y * 255.f, out.z * 255.f, out.w * 255.f);
            }
            default:
                return tex->read2d<unsigned>(make_uint2(x, y));
        }
    }

    uint4 fallback::texture_read_3d_uint(const FallbackTextureView* tex, uint x, uint y, uint z) noexcept
    {
        switch (tex->storage())
        {
            case PixelStorage::BC7:
            {
                LUISA_ERROR("Block compression doesn't work for 3D texture");
                return make_uint4(0);
            }
            case PixelStorage::BC6:
            {
                LUISA_ERROR("Block compression doesn't work for 3D texture");
                return make_uint4(0);
            }
            default:
                return tex->read3d<unsigned>(make_uint3(x, y, z));
        }
    }

    float4 fallback::texture_read_2d_float(const FallbackTextureView* tex, uint x, uint y) noexcept
    {
        switch (tex->storage())
        {
            case PixelStorage::BC7:
            {
                float4 out;
                luisa_bc7_read(tex, x, y, out);
                return out;
            }
            case PixelStorage::BC6:
            {
                float4 out;
                luisa_bc6h_read(tex, x, y, out);
                return out;
            }
            default:
                return tex->read2d<float>(make_uint2(x, y));
        }
    }

    void fallback::texture_write_2d_float(const FallbackTextureView* tex, uint x, uint y, float4 value) noexcept
    {
        switch (tex->storage())
        {
            case PixelStorage::BC7:
            {
                LUISA_ERROR("cannot write to BC texture");
                break;
            }
            case PixelStorage::BC6:
            {
                LUISA_ERROR("cannot write to BC texture");
                break;
            }
            default:
                return tex->write2d<float>(make_uint2(x, y), value);
        }
    }
    void fallback::texture_write_2d_uint(const FallbackTextureView* tex, uint x, uint y, uint4 value) noexcept
    {
        switch (tex->storage())
        {
            case PixelStorage::BC7:
            {
                LUISA_ERROR("cannot write to BC texture");
                break;
            }
            case PixelStorage::BC6:
            {
                LUISA_ERROR("cannot write to BC texture");
                break;
            }
            default:
                return tex->write2d<uint>(make_uint2(x, y), value);
        }
    }

    float4 fallback::texture_read_3d_float(const FallbackTextureView* tex, uint x, uint y, uint z) noexcept
    {
        switch (tex->storage())
        {
            case PixelStorage::BC7:
            {
                LUISA_ERROR("Block compression doesn't work for 3D texture");
                return make_float4(0.f);
            }
            case PixelStorage::BC6:
            {
                LUISA_ERROR("Block compression doesn't work for 3D texture");
                return make_float4(0.f);
            }
            default:
                return tex->read3d<float>(make_uint3(x, y, z));
        }
    }


    float4 fallback::bindless_texture_2d_read(const FallbackTexture* tex, uint level, uint x, uint y) noexcept
    {
        auto view = tex->view(level);
        return texture_read_2d_float(&view, x, y);
    }

    float4 fallback::bindless_texture_3d_read(const FallbackTexture* tex, uint level, uint x, uint y, uint z) noexcept
    {
        auto view = tex->view(level);
        return texture_read_3d_float(&view, x, y, z);
    }


    //2D sampling
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
                                                   const float2& uv) noexcept
    {
        auto size = make_float2(view->size2d());
        auto c = make_uint2(texture_coord_point(address, uv, size));
        return texture_read_2d_float(view, c.x, c.y);
    }

    [[nodiscard]] inline auto texture_coord_linear(Sampler::Address address, float2 uv, const float2& size) noexcept
    {
        auto s = make_float2(size);
        auto inv_s = 1.f / s;
        auto c_min = texture_coord_point(address, uv - .5f * inv_s, s);
        auto c_max = texture_coord_point(address, uv + .5f * inv_s, s);
        return std::make_pair(luisa::min(c_min, c_max), luisa::max(c_min, c_max));
    }

    [[nodiscard]] inline auto texture_sample_linear(FallbackTextureView* view, Sampler::Address address,
                                                    const float2& uv) noexcept
    {
        auto size = make_float2(view->size2d());
        auto [st_min, st_max] = texture_coord_linear(address, uv, size);
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

    float4 fallback::bindless_texture_2d_sample(const FallbackTexture* tex, uint sampler, float u, float v) noexcept
    {
        auto s = Sampler::decode(sampler);
        auto view = tex->view(0);
        return s.filter() == Sampler::Filter::POINT
                   ? texture_sample_point(&view, s.address(), make_float2(u, v))
                   : texture_sample_linear(&view, s.address(), make_float2(u, v));
    }

    float4 bindless_texture_2d_sample_level(const FallbackTexture* tex, uint sampler, float u, float v,
                                            float lod) noexcept
    {
        auto s = Sampler::decode(sampler);
        auto filter = s.filter();
        if (lod <= 0.f || tex->mip_levels() == 0u ||
            filter == Sampler::Filter::POINT)
        {
            return bindless_texture_2d_sample(tex, sampler, u, v);
        }
        auto level0 = std::min(static_cast<uint32_t>(lod),
                               tex->mip_levels() - 1u);
        auto view0 = tex->view(level0);
        auto v0 = texture_sample_linear(
            &view0, s.address(), make_float2(u, v));
        if (level0 == tex->mip_levels() - 1u ||
            filter == Sampler::Filter::LINEAR_POINT)
        {
            return v0;
        }
        auto view1 = tex->view(level0 + 1);
        auto v1 = texture_sample_linear(
            &view1, s.address(), make_float2(u, v));
        return luisa::lerp(v0, v1, luisa::fract(lod));
    }

    //swfly: im too lazy to do this. complete it someday
    float4 bindless_texture_2d_sample_grad(const FallbackTexture* tex, uint sampler, float u, float v, int64_t dpdx,
                                           int64_t dpdy) noexcept
    {
        return bindless_texture_2d_sample(tex, sampler, u, v);
    }

    //3D sampling
    [[nodiscard]] inline auto texture_sample_point(FallbackTextureView* view, Sampler::Address address,
                                                   float3 uv) noexcept
    {
        auto size = make_float3(view->size3d());
        auto c = make_uint3(texture_coord_point(address, uv, size));
        return texture_read_3d_float(view, c.x, c.y, c.z);
    }

    [[nodiscard]] inline auto texture_coord_linear(Sampler::Address address, float3 uv, float3 size) noexcept
    {
        auto s = make_float3(size);
        auto inv_s = 1.f / s;
        auto c_min = texture_coord_point(address, uv - .5f * inv_s, s);
        auto c_max = texture_coord_point(address, uv + .5f * inv_s, s);
        return std::make_pair(luisa::min(c_min, c_max), luisa::max(c_min, c_max));
    }

    [[nodiscard]] inline auto texture_sample_linear(FallbackTextureView* view, Sampler::Address address,
                                                    float3 uvw) noexcept
    {
        auto size = make_float3(view->size3d());
        auto [st_min, st_max] = texture_coord_linear(address, uvw, size);
        auto t = luisa::fract(st_max);
        auto c0 = make_uint3(st_min);
        auto c1 = make_uint3(st_max);
        auto v000 = texture_read_3d_float(view, c0.x, c0.y, c0.z);
        auto v001 = texture_read_3d_float(view, c1.x, c0.y, c0.z);
        auto v010 = texture_read_3d_float(view, c0.x, c1.y, c0.z);
        auto v011 = texture_read_3d_float(view, c1.x, c1.y, c0.z);
        auto v100 = texture_read_3d_float(view, c0.x, c0.y, c1.z);
        auto v101 = texture_read_3d_float(view, c1.x, c0.y, c1.z);
        auto v110 = texture_read_3d_float(view, c0.x, c1.y, c1.z);
        auto v111 = texture_read_3d_float(view, c1.x, c1.y, c1.z);
        return luisa::lerp(
            luisa::lerp(luisa::lerp(v000, v001, t.x),
                        luisa::lerp(v010, v011, t.x), t.y),
            luisa::lerp(luisa::lerp(v100, v101, t.x),
                        luisa::lerp(v110, v111, t.x), t.y),
            t.z);
    }

    float4 fallback::bindless_texture_3d_sample(const FallbackTexture* tex, uint sampler, float u, float v,
                                                float w) noexcept
    {
        auto s = Sampler::decode(sampler);
        auto view = tex->view(0);
        return s.filter() == Sampler::Filter::POINT
                   ? texture_sample_point(&view, s.address(), make_float3(u, v, w))
                   : texture_sample_linear(&view, s.address(), make_float3(u, v, w));
    }

    float4 bindless_texture_3d_sample_level(const FallbackTexture* tex, uint sampler, float u, float v, float w,
                                            float lod) noexcept
    {
        auto s = Sampler::decode(sampler);
        auto filter = s.filter();
        if (lod <= 0.f || tex->mip_levels() == 0u ||
            filter == Sampler::Filter::POINT)
        {
            return bindless_texture_3d_sample(tex, sampler, u, v, w);
        }
        auto level0 = std::min(static_cast<uint32_t>(lod),
                               tex->mip_levels() - 1u);
        auto view0 = tex->view(level0);
        auto v0 = texture_sample_linear(
            &view0, s.address(), make_float3(u, v, w));
        if (level0 == tex->mip_levels() - 1u ||
            filter == Sampler::Filter::LINEAR_POINT)
        {
            return v0;
        }
        auto view1 = tex->view(level0 + 1);
        auto v1 = texture_sample_linear(
            &view1, s.address(), make_float3(u, v, w));
        return luisa::lerp(v0, v1, luisa::fract(lod));
    }


    //swfly: im too lazy to do this. complete it someday
    float4 bindless_texture_3d_sample_grad(const FallbackTexture* tex, uint sampler, float u, float v, float w,
                                           int64_t dudxy, int64_t dvdxy, int64_t dwdxy) noexcept
    {
        return bindless_texture_3d_sample(tex, sampler, u, v, w);
    }
} // namespace luisa::compute::fallback
