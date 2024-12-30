#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

enum class ResourceWriteOp {

    // buffer write operations
    BUFFER_WRITE,     /// [(buffer, index, value) -> void]: writes value into the index-th element of buffer
    BYTE_BUFFER_WRITE,/// [(buffer, byte_index, value) -> void]: writes value into the index-th element of buffer

    // texture write operations
    TEXTURE2D_WRITE,/// [(texture, coord, value) -> void]
    TEXTURE3D_WRITE,/// [(texture, coord, value) -> void]

    // bindless array write operations
    BINDLESS_BUFFER_WRITE,     // (bindless_array, index: uint, elem_index: uint, value: T) -> void
    BINDLESS_BYTE_BUFFER_WRITE,// (bindless_array, index: uint, offset_bytes: uint64, value: T) -> void

    // low-level pointer operations, for akari
    DEVICE_ADDRESS_WRITE,// (address: uint64, value: T) -> void

    // ray tracing
    RAY_TRACING_SET_INSTANCE_TRANSFORM,      // (Accel, uint, float4x4)
    RAY_TRACING_SET_INSTANCE_VISIBILITY_MASK,// (Accel, uint, uint)
    RAY_TRACING_SET_INSTANCE_OPACITY,        // (Accel, uint, bool)
    RAY_TRACING_SET_INSTANCE_USER_ID,        // (Accel, uint, uint)
    RAY_TRACING_SET_INSTANCE_MOTION_MATRIX,  // (Accel, index: uint, key: uint, transform: float4x4)
    RAY_TRACING_SET_INSTANCE_MOTION_SRT,     // (Accel, index: uint, key: uint, transform: SRT)

    // indirect dispatch
    INDIRECT_DISPATCH_SET_KERNEL,// (Buffer, uint offset, uint3 block_size, uint3 dispatch_size, uint kernel_id)
    INDIRECT_DISPATCH_SET_COUNT, // (Buffer, uint count)
};

[[nodiscard]] LC_XIR_API luisa::string_view to_string(ResourceWriteOp op) noexcept;
[[nodiscard]] LC_XIR_API ResourceWriteOp resource_write_op_from_string(luisa::string_view name) noexcept;

class LC_XIR_API ResourceWriteInst final : public DerivedInstruction<DerivedInstructionTag::RESOURCE_WRITE>,
                                           public InstructionOpMixin<ResourceWriteOp> {
public:
    explicit ResourceWriteInst(ResourceWriteOp op = {},
                               luisa::span<Value *const> operands = {}) noexcept;
};

}// namespace luisa::compute::xir
