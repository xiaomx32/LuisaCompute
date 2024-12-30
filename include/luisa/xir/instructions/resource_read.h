#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

enum class ResourceReadOp {
    BUFFER_READ,     /// [(buffer, index) -> value]: reads the index-th element in buffer
    BYTE_BUFFER_READ,/// [(buffer, byte_index) -> value]: reads the index-th element in buffer

    TEXTURE2D_READ,/// [(texture, coord) -> value]
    TEXTURE3D_READ,/// [(texture, coord) -> value]

    BINDLESS_BUFFER_READ,     // (bindless_array, index: uint, elem_index: uint) -> T
    BINDLESS_BYTE_BUFFER_READ,// (bindless_array, index: uint, offset_bytes: uint64) -> T

    BINDLESS_TEXTURE2D_READ,      // (bindless_array, index: uint, coord: uint2): float4
    BINDLESS_TEXTURE3D_READ,      // (bindless_array, index: uint, coord: uint3): float4
    BINDLESS_TEXTURE2D_READ_LEVEL,// (bindless_array, index: uint, coord: uint2, level: uint): float4
    BINDLESS_TEXTURE3D_READ_LEVEL,// (bindless_array, index: uint, coord: uint3, level: uint): float4

    DEVICE_ADDRESS_READ,// (address: uint64) -> value: T
};

[[nodiscard]] LC_XIR_API luisa::string_view to_string(ResourceReadOp op) noexcept;
[[nodiscard]] LC_XIR_API ResourceReadOp resource_read_op_from_string(luisa::string_view name) noexcept;

class LC_XIR_API ResourceReadInst final : public DerivedInstruction<DerivedInstructionTag::RESOURCE_READ>,
                                          public InstructionOpMixin<ResourceReadOp> {
public:
    explicit ResourceReadInst(const Type *type = nullptr, ResourceReadOp op = {},
                              luisa::span<Value *const> operands = {}) noexcept;
};

}// namespace luisa::compute::xir
