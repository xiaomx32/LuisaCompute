#pragma once
#include <luisa/runtime/rhi/device_interface.h>

namespace luisa::compute {
class DXHDRExt : public DeviceExtension {
public:
    enum class DisplayCurve {
        sRGB = 0,// The display expects an sRGB signal.
        ST2084,  // The display expects an HDR10 signal.
        None     // The display expects a linear signal.
    };
    enum class SwapChainBitDepth {
        _8 = 0,
        _10,
        _16,
        SwapChainBitDepthCount
    };
    struct DisplayChromaticities {
        float RedX;
        float RedY;
        float GreenX;
        float GreenY;
        float BlueX;
        float BlueY;
        float WhiteX;
        float WhiteY;
    };
    struct DXSwapchainOption {
        uint64_t window;
        uint2 size;
        PixelStorage storage = PixelStorage::HALF4;
        bool wants_vsync = true;
        uint back_buffer_count = 2;
    };

    [[nodiscard]] virtual SwapchainCreationInfo create_swapchain(
        const DXSwapchainOption &option,
        uint64_t stream_handle) noexcept = 0;
    virtual void set_hdr_meta_data(
        uint64_t swapchain_handle,
        float max_output_nits = 1000.0f,
        float min_output_nits = 0.001f,
        float max_cll = 2000.0f,
        float max_fall = 500.0f,
        const DXHDRExt::DisplayChromaticities *custom_chroma = nullptr) noexcept = 0;
    static constexpr luisa::string_view name = "DXHDRExt";
protected:
    ~DXHDRExt() = default;
};
}// namespace luisa::compute