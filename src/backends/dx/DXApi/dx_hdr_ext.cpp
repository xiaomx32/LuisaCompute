#include <DXApi/dx_hdr_ext.hpp>
#include <DXApi/TypeCheck.h>
#include <d3d12.h>
#include <dxgi1_5.h>
#include <DXApi/LCSwapChain.h>
#include <Resource/TextureBase.h>
namespace lc::dx {
using namespace luisa::compute;
namespace dx_hdr_ext_detail {

DXHDRExt::DisplayCurve EnsureSwapChainColorSpace(
    IDXGISwapChain4 *swapChain,
    DXGI_COLOR_SPACE_TYPE &currentSwapChainColorSpace,
    DXHDRExt::SwapChainBitDepth swapChainBitDepth,
    bool enableST2084) {
    DXHDRExt::DisplayCurve result{DXHDRExt::DisplayCurve::None};
    DXGI_COLOR_SPACE_TYPE colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    switch (swapChainBitDepth) {
        case DXHDRExt::SwapChainBitDepth::_8:
            result = DXHDRExt::DisplayCurve::sRGB;
            break;

        case DXHDRExt::SwapChainBitDepth::_10:
            colorSpace = enableST2084 ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
            result = enableST2084 ? DXHDRExt::DisplayCurve::ST2084 : DXHDRExt::DisplayCurve::sRGB;
            break;

        case DXHDRExt::SwapChainBitDepth::_16:
            colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
            result = DXHDRExt::DisplayCurve::None;
            break;
    }

    if (currentSwapChainColorSpace != colorSpace) {
        UINT colorSpaceSupport = 0;
        if (SUCCEEDED(swapChain->CheckColorSpaceSupport(colorSpace, &colorSpaceSupport)) &&
            ((colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) == DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)) {
            ThrowIfFailed(swapChain->SetColorSpace1(colorSpace));
            currentSwapChainColorSpace = colorSpace;
        }
    }
    return result;
}
DXHDRExt::DisplayChromaticities SetHDRMetaData(
    DXGI_COLOR_SPACE_TYPE &colorSpace,
    LCSwapChain *swapchain,
    bool hdr_support,
    float MaxOutputNits /*=1000.0f*/,
    float MinOutputNits /*=0.001f*/,
    float MaxCLL /*=2000.0f*/,
    float MaxFALL /*=500.0f*/,
    const DXHDRExt::DisplayChromaticities *Chroma) {
    if (!swapchain) {
        return {};
    }

    // Clean the hdr metadata if the display doesn't support HDR
    if (!hdr_support) {
        ThrowIfFailed(swapchain->swapChain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_NONE, 0, nullptr));
        return {};
    }

    static const DXHDRExt::DisplayChromaticities DisplayChromaticityList[] =
        {
            {0.64000f, 0.33000f, 0.30000f, 0.60000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f},// Display Gamut Rec709
            {0.70800f, 0.29200f, 0.17000f, 0.79700f, 0.13100f, 0.04600f, 0.31270f, 0.32900f},// Display Gamut Rec2020
        };

    // Select the chromaticity based on HDR format of the DWM.
    DXHDRExt::SwapChainBitDepth hitDepth;
    switch (swapchain->format) {
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
            hitDepth = DXHDRExt::SwapChainBitDepth::_10;
            break;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
            hitDepth = DXHDRExt::SwapChainBitDepth::_16;
            break;
        default:
            hitDepth = DXHDRExt::SwapChainBitDepth::_8;
            break;
    }

    EnsureSwapChainColorSpace(swapchain->swapChain, colorSpace, hitDepth, hdr_support);
    int selectedChroma = 0;
    if (swapchain->format == DXGI_FORMAT_R16G16B16A16_FLOAT && colorSpace == DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709) {
        selectedChroma = 0;
    } else if (hitDepth == DXHDRExt::SwapChainBitDepth::_10 && colorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) {
        selectedChroma = 1;
    } else {
        // Reset the metadata since this is not a supported HDR format.
        ThrowIfFailed(swapchain->swapChain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_NONE, 0, nullptr));
        return {};
    }

    // Set HDR meta data
    if (!Chroma)
        Chroma = &DisplayChromaticityList[selectedChroma];
    DXGI_HDR_METADATA_HDR10 HDR10MetaData = {};
    HDR10MetaData.RedPrimary[0] = static_cast<UINT16>(Chroma->RedX * 50000.0f);
    HDR10MetaData.RedPrimary[1] = static_cast<UINT16>(Chroma->RedY * 50000.0f);
    HDR10MetaData.GreenPrimary[0] = static_cast<UINT16>(Chroma->GreenX * 50000.0f);
    HDR10MetaData.GreenPrimary[1] = static_cast<UINT16>(Chroma->GreenY * 50000.0f);
    HDR10MetaData.BluePrimary[0] = static_cast<UINT16>(Chroma->BlueX * 50000.0f);
    HDR10MetaData.BluePrimary[1] = static_cast<UINT16>(Chroma->BlueY * 50000.0f);
    HDR10MetaData.WhitePoint[0] = static_cast<UINT16>(Chroma->WhiteX * 50000.0f);
    HDR10MetaData.WhitePoint[1] = static_cast<UINT16>(Chroma->WhiteY * 50000.0f);
    HDR10MetaData.MaxMasteringLuminance = static_cast<UINT>(MaxOutputNits * 10000.0f);
    HDR10MetaData.MinMasteringLuminance = static_cast<UINT>(MinOutputNits * 10000.0f);
    HDR10MetaData.MaxContentLightLevel = static_cast<UINT16>(MaxCLL);
    HDR10MetaData.MaxFrameAverageLightLevel = static_cast<UINT16>(MaxFALL);
    ThrowIfFailed(swapchain->swapChain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), &HDR10MetaData));
    return *Chroma;
}
}// namespace dx_hdr_ext_detail
DXHDRExtImpl::DXHDRExtImpl(LCDevice *lc_device) : _lc_device(lc_device) {}
DXHDRExtImpl::~DXHDRExtImpl() = default;

SwapchainCreationInfo DXHDRExtImpl::create_swapchain(
    const DXSwapchainOption &option,
    uint64_t stream_handle) noexcept {
    auto queue = reinterpret_cast<CmdQueueBase *>(stream_handle);
    if (queue->Tag() != CmdQueueTag::MainCmd) [[unlikely]] {
        LUISA_ERROR("swapchain not allowed in Direct-Storage.");
    }
    SwapchainCreationInfo info;
    auto res = new LCSwapChain(
        &(_lc_device->nativeDevice),
        &reinterpret_cast<LCCmdBuffer *>(stream_handle)->queue,
        _lc_device->nativeDevice.defaultAllocator.get(),
        reinterpret_cast<HWND>(option.window),
        option.size.x,
        option.size.y,
        static_cast<DXGI_FORMAT>(TextureBase::ToGFXFormat(pixel_storage_to_format<float>(option.storage))),
        option.wants_vsync,
        option.back_buffer_count);
    info.handle = resource_to_handle(res);
    info.native_handle = res->swapChain.Get();
    info.storage = option.storage;
    return info;
}

auto DXHDRExtImpl::set_hdr_meta_data(
    uint64_t swapchain_handle,
    float max_output_nits,
    float min_output_nits,
    float max_cll,
    float max_fall,
    const DXHDRExt::DisplayChromaticities *custom_chroma) noexcept -> Meta {
    DXGI_COLOR_SPACE_TYPE color_space = DXGI_COLOR_SPACE_CUSTOM;
    auto chroma = dx_hdr_ext_detail::SetHDRMetaData(
        color_space,
        reinterpret_cast<LCSwapChain *>(swapchain_handle),
        true,
        max_output_nits,
        min_output_nits,
        max_cll,
        max_fall,
        custom_chroma);
    return {
        static_cast<ColorSpace>(color_space),
        chroma};
}
}// namespace lc::dx