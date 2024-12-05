//
// Created by Mike Smith on 2022/5/23.
//

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/Verifier.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

#include <luisa/core/stl.h>
#include <luisa/core/logging.h>
#include "fallback_stream.h"
#include "fallback_device.h"
#include "fallback_texture.h"
#include "fallback_mesh.h"
#include "fallback_accel.h"
#include "fallback_bindless_array.h"
#include "fallback_shader.h"
#include "fallback_buffer.h"
#include "fallback_swapchain.h"

//#include "llvm_event.h"
//#include "llvm_shader.h"
//#include "llvm_codegen.h"

#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>

//#include "fallback_texture_sampling_wrapper.ll";



namespace luisa::compute::fallback {

static void loadLLVMModuleFromString(::llvm::orc::LLJIT &jit, const char *ir_string) {
    // Create an LLVM context
    auto llvm_ctx = std::make_unique<llvm::LLVMContext>();
    // Wrap the string in a MemoryBuffer
    auto buffer = ::llvm::MemoryBuffer::getMemBuffer(ir_string, "embedded_ir");
    // Parse the IR
    ::llvm::SMDiagnostic err;
    auto module = ::llvm::parseIR(buffer->getMemBufferRef(), err, *llvm_ctx);
    if (!module) {
        throw std::runtime_error("Failed to parse embedded LLVM IR: " + err.getMessage().str());
    }
    // Wrap the module in a ThreadSafeModule
    auto tsm = ::llvm::orc::ThreadSafeModule(std::move(module), std::move(llvm_ctx));

    // Add the module to the JIT
    if (auto err = jit.addIRModule(std::move(tsm))) {
        ::llvm::handleAllErrors(std::move(err), [](const ::llvm::ErrorInfoBase &err) {
            LUISA_WARNING_WITH_LOCATION("LLJIT::addIRModule(): {}", err.message());
        });
    }
}

extern "C" int my_function(int x, int y) {
    printf("my_function called with arguments: %d, %d\n", x, y);
    return x + y;
}
FallbackDevice::FallbackDevice(Context &&ctx) noexcept
    : DeviceInterface{std::move(ctx)},
      _rtc_device{rtcNewDevice(nullptr)} {
    static std::once_flag flag;
    std::call_once(flag, [] {
        ::llvm::InitializeNativeTarget();
        ::llvm::InitializeNativeTargetAsmPrinter();
    });
}

void *FallbackDevice::native_handle() const noexcept {
    return reinterpret_cast<void *>(reinterpret_cast<uint64_t>(this));
}

void FallbackDevice::destroy_buffer(uint64_t handle) noexcept {
    luisa::deallocate_with_allocator(reinterpret_cast<FallbackBuffer *>(handle));
}

void FallbackDevice::destroy_texture(uint64_t handle) noexcept {
    luisa::delete_with_allocator(reinterpret_cast<FallbackTexture *>(handle));
}

void FallbackDevice::destroy_bindless_array(uint64_t handle) noexcept {
    luisa::delete_with_allocator(reinterpret_cast<FallbackBindlessArray *>(handle));
}

void FallbackDevice::destroy_stream(uint64_t handle) noexcept {
    luisa::delete_with_allocator(reinterpret_cast<FallbackStream *>(handle));
}

void FallbackDevice::synchronize_stream(uint64_t stream_handle) noexcept {
    reinterpret_cast<FallbackStream *>(stream_handle)->synchronize();
}

void FallbackDevice::destroy_shader(uint64_t handle) noexcept {
    //luisa::delete_with_allocator(reinterpret_cast<LLVMShader *>(handle));
}

void FallbackDevice::destroy_event(uint64_t handle) noexcept {
    //luisa::delete_with_allocator(reinterpret_cast<LLVMEvent *>(handle));
}

void FallbackDevice::destroy_mesh(uint64_t handle) noexcept {
    luisa::delete_with_allocator(reinterpret_cast<FallbackMesh *>(handle));
}

void FallbackDevice::destroy_accel(uint64_t handle) noexcept {
    luisa::delete_with_allocator(reinterpret_cast<FallbackAccel *>(handle));
}

void FallbackDevice::destroy_swap_chain(uint64_t handle) noexcept {
	auto b = reinterpret_cast<FallbackSwapchain*>(handle);
	luisa::deallocate_with_allocator(b);
}

void FallbackDevice::present_display_in_stream(
    uint64_t stream_handle, uint64_t swap_chain_handle, uint64_t image_handle) noexcept {

	auto b = reinterpret_cast<FallbackSwapchain*>(swap_chain_handle);
	b->Present(reinterpret_cast<void*>(image_handle));
}

FallbackDevice::~FallbackDevice() noexcept {
    rtcReleaseDevice(_rtc_device);
}

uint FallbackDevice::compute_warp_size() const noexcept {
    return 1;
}

BufferCreationInfo FallbackDevice::create_buffer(const Type *element, size_t elem_count, void *external_memory) noexcept {

    BufferCreationInfo info{};

    if (element == Type::of<void>())
    {
        //Byte buffer...
    info.element_stride = 1u;
    }
    else
    {
    info.element_stride = element->size();
    }
    info.total_size_bytes = info.element_stride * elem_count;
	auto buffer = luisa::new_with_allocator<FallbackBuffer>(elem_count, info.element_stride);
    info.handle = reinterpret_cast<uint64_t>(buffer);
    info.native_handle = reinterpret_cast<void *>(info.handle);
    return info;
}

BufferCreationInfo FallbackDevice::create_buffer(const ir::CArc<ir::Type> *element, size_t elem_count, void *external_memory) noexcept {
    return BufferCreationInfo();
}

ResourceCreationInfo FallbackDevice::create_texture(PixelFormat format, uint dimension, uint width, uint height, uint depth, uint mipmap_levels, bool simultaneous_access, bool allow_raster_target) noexcept {
    ResourceCreationInfo info{};
    auto texture = luisa::new_with_allocator<FallbackTexture>(
        pixel_format_to_storage(format), dimension,
        make_uint3(width, height, depth), mipmap_levels);
    info.handle = reinterpret_cast<uint64_t>(texture);
    return info;
}

ResourceCreationInfo FallbackDevice::create_stream(StreamTag stream_tag) noexcept {
    return ResourceCreationInfo{
        .handle = reinterpret_cast<uint64_t>(luisa::new_with_allocator<FallbackStream>())};
}

void FallbackDevice::dispatch(uint64_t stream_handle, CommandList &&list) noexcept {
    auto stream = reinterpret_cast<FallbackStream *>(stream_handle);
    stream->dispatch(std::move(list));
}

void FallbackDevice::set_stream_log_callback(uint64_t stream_handle, const DeviceInterface::StreamLogCallback &callback) noexcept {
    DeviceInterface::set_stream_log_callback(stream_handle, callback);
}

SwapchainCreationInfo FallbackDevice::create_swapchain(const SwapchainOption &option, uint64_t stream_handle) noexcept {

	auto sc = luisa::new_with_allocator<FallbackSwapchain>(option);
	return SwapchainCreationInfo{
		ResourceCreationInfo{.handle = reinterpret_cast<uint64_t>(sc),
							 .native_handle = nullptr},
		(option.wants_hdr ? PixelStorage::FLOAT4 : PixelStorage::BYTE4)
	};
}

ShaderCreationInfo FallbackDevice::create_shader(const ShaderOption &option, Function kernel) noexcept {
    return ShaderCreationInfo{
        ResourceCreationInfo{
            .handle = reinterpret_cast<uint64_t>(luisa::new_with_allocator<FallbackShader>(option, kernel))}};
    return ShaderCreationInfo();
}

ShaderCreationInfo FallbackDevice::create_shader(const ShaderOption &option, const ir::KernelModule *kernel) noexcept {
    return ShaderCreationInfo();
}

ShaderCreationInfo FallbackDevice::create_shader(const ShaderOption &option, const ir_v2::KernelModule &kernel) noexcept {
    return DeviceInterface::create_shader(option, kernel);
}

ShaderCreationInfo FallbackDevice::load_shader(luisa::string_view name, luisa::span<const Type *const> arg_types) noexcept {
    return ShaderCreationInfo();
}

Usage FallbackDevice::shader_argument_usage(uint64_t handle, size_t index) noexcept {
    return Usage::READ;
}

void FallbackDevice::signal_event(uint64_t handle, uint64_t stream_handle, uint64_t fence_value) noexcept {
    reinterpret_cast<FallbackStream *>(stream_handle)->signal(reinterpret_cast<LLVMEvent *>(handle));
}

void FallbackDevice::wait_event(uint64_t handle, uint64_t stream_handle, uint64_t fence_value) noexcept {
    reinterpret_cast<FallbackStream *>(stream_handle)->wait(reinterpret_cast<LLVMEvent *>(handle));
}

bool FallbackDevice::is_event_completed(uint64_t handle, uint64_t fence_value) const noexcept {
    return false;
}

void FallbackDevice::synchronize_event(uint64_t handle, uint64_t fence_value) noexcept {
    //reinterpret_cast<LLVMEvent *>(handle)->wait();
}

ResourceCreationInfo FallbackDevice::create_mesh(const AccelOption &option) noexcept {
    return {
        .handle = reinterpret_cast<uint64_t>(luisa::new_with_allocator<FallbackMesh>(_rtc_device, option.hint)),
        .native_handle = nullptr};
}

ResourceCreationInfo FallbackDevice::create_procedural_primitive(const AccelOption &option) noexcept {
    return ResourceCreationInfo();
}

void FallbackDevice::destroy_procedural_primitive(uint64_t handle) noexcept {
}

ResourceCreationInfo FallbackDevice::create_curve(const AccelOption &option) noexcept {
    return DeviceInterface::create_curve(option);
}

void FallbackDevice::destroy_curve(uint64_t handle) noexcept {
    DeviceInterface::destroy_curve(handle);
}

ResourceCreationInfo FallbackDevice::create_motion_instance(const AccelMotionOption &option) noexcept {
    return DeviceInterface::create_motion_instance(option);
}

void FallbackDevice::destroy_motion_instance(uint64_t handle) noexcept {
    DeviceInterface::destroy_motion_instance(handle);
}

ResourceCreationInfo FallbackDevice::create_accel(const AccelOption &option) noexcept {
    return ResourceCreationInfo{
        .handle = reinterpret_cast<uint64_t>(luisa::new_with_allocator<FallbackAccel>(_rtc_device, option.hint)),
        .native_handle = nullptr};
}

string FallbackDevice::query(luisa::string_view property) noexcept {
    return DeviceInterface::query(property);
}

DeviceExtension *FallbackDevice::extension(luisa::string_view name) noexcept {
    return DeviceInterface::extension(name);
}

void FallbackDevice::set_name(luisa::compute::Resource::Tag resource_tag, uint64_t resource_handle, luisa::string_view name) noexcept {
}

SparseBufferCreationInfo FallbackDevice::create_sparse_buffer(const Type *element, size_t elem_count) noexcept {
    return DeviceInterface::create_sparse_buffer(element, elem_count);
}

ResourceCreationInfo FallbackDevice::allocate_sparse_buffer_heap(size_t byte_size) noexcept {
    return DeviceInterface::allocate_sparse_buffer_heap(byte_size);
}

void FallbackDevice::deallocate_sparse_buffer_heap(uint64_t handle) noexcept {
    DeviceInterface::deallocate_sparse_buffer_heap(handle);
}

void FallbackDevice::update_sparse_resources(uint64_t stream_handle, vector<SparseUpdateTile> &&textures_update) noexcept {
    DeviceInterface::update_sparse_resources(stream_handle, std::move(textures_update));
}

void FallbackDevice::destroy_sparse_buffer(uint64_t handle) noexcept {
    DeviceInterface::destroy_sparse_buffer(handle);
}

ResourceCreationInfo FallbackDevice::allocate_sparse_texture_heap(size_t byte_size, bool is_compressed_type) noexcept {
    return DeviceInterface::allocate_sparse_texture_heap(byte_size, is_compressed_type);
}

void FallbackDevice::deallocate_sparse_texture_heap(uint64_t handle) noexcept {
    DeviceInterface::deallocate_sparse_texture_heap(handle);
}

SparseTextureCreationInfo FallbackDevice::create_sparse_texture(PixelFormat format, uint dimension, uint width, uint height, uint depth, uint mipmap_levels, bool simultaneous_access) noexcept {
    return DeviceInterface::create_sparse_texture(format, dimension, width, height, depth, mipmap_levels, simultaneous_access);
}

void FallbackDevice::destroy_sparse_texture(uint64_t handle) noexcept {
    DeviceInterface::destroy_sparse_texture(handle);
}

ResourceCreationInfo FallbackDevice::create_bindless_array(size_t size) noexcept {
    ResourceCreationInfo info{};
    auto array = luisa::new_with_allocator<FallbackBindlessArray>(size);
    info.handle = reinterpret_cast<uint64_t>(array);
    return info;
}

ResourceCreationInfo FallbackDevice::create_event() noexcept {
    return ResourceCreationInfo{};
    //    return ResourceCreationInfo
    //    {
    //        .handle = reinterpret_cast<uint64_t>(luisa::new_with_allocator<LLVMEvent>())
    //    };
}

}// namespace luisa::compute::fallback

LUISA_EXPORT_API luisa::compute::DeviceInterface *create(luisa::compute::Context &&ctx, std::string_view) noexcept {
    return luisa::new_with_allocator<luisa::compute::fallback::FallbackDevice>(std::move(ctx));
}

LUISA_EXPORT_API void destroy(luisa::compute::DeviceInterface *device) noexcept {
    luisa::delete_with_allocator(device);
}

LUISA_EXPORT_API void backend_device_names(luisa::vector<luisa::string> &names) noexcept {
    names.clear();
    names.emplace_back(luisa::cpu_name());
}

