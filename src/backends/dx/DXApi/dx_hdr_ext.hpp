#pragma once
#include <luisa/backends/ext/dx_hdr_ext_interface.h>
#include <DXApi/LCDevice.h>
namespace lc::dx {
class DXHDRExtImpl : public luisa::compute::DXHDRExt {
    LCDevice *_lc_device;
public:
    DXHDRExtImpl(LCDevice *lc_device);
    ~DXHDRExtImpl();
    SwapchainCreationInfo create_swapchain(
        const DXSwapchainOption &option,
        uint64_t stream_handle) noexcept override;
    Meta set_hdr_meta_data(
        uint64_t swapchain_handle,
        float max_output_nits = 1000.0f,
        float min_output_nits = 0.001f,
        float max_cll = 2000.0f,
        float max_fall = 500.0f,
        const DXHDRExt::DisplayChromaticities *custom_chroma = nullptr) noexcept override;
};
}// namespace lc::dx