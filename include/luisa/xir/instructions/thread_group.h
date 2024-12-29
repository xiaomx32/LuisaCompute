#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

enum class ThreadGroupOp {

    // shader execution re-ordering
    SHADER_EXECUTION_REORDER,// (uint hint, uint hint_bits): void

    RASTER_QUAD_DDX,// (arg: floatN): floatN
    RASTER_QUAD_DDY,// (arg: floatN): floatN

    // warp operations
    WARP_IS_FIRST_ACTIVE_LANE,  // (): bool
    WARP_FIRST_ACTIVE_LANE,     // (): uint
    WARP_ACTIVE_ALL_EQUAL,      // (scalar/vector): boolN
    WARP_ACTIVE_BIT_AND,        // (intN): intN
    WARP_ACTIVE_BIT_OR,         // (intN): intN
    WARP_ACTIVE_BIT_XOR,        // (intN): intN
    WARP_ACTIVE_COUNT_BITS,     // (bool): uint
    WARP_ACTIVE_MAX,            // (type: scalar/vector): type
    WARP_ACTIVE_MIN,            // (type: scalar/vector): type
    WARP_ACTIVE_PRODUCT,        // (type: scalar/vector): type
    WARP_ACTIVE_SUM,            // (type: scalar/vector): type
    WARP_ACTIVE_ALL,            // (bool): bool
    WARP_ACTIVE_ANY,            // (bool): bool
    WARP_ACTIVE_BIT_MASK,       // (bool): uint4 (uint4 contained 128-bit)
    WARP_PREFIX_COUNT_BITS,     // (bool): uint (count bits before this lane)
    WARP_PREFIX_SUM,            // (type: scalar/vector): type (sum lanes before this lane)
    WARP_PREFIX_PRODUCT,        // (type: scalar/vector): type (multiply lanes before this lane)
    WARP_READ_LANE,             // (type: scalar/vector/matrix, index: uint): type (read this variable's value at this lane)
    WARP_READ_FIRST_ACTIVE_LANE,// (type: scalar/vector/matrix): type (read this variable's value at the first lane)

    // block synchronization
    SYNCHRONIZE_BLOCK,// ()
};

[[nodiscard]] constexpr auto to_string(ThreadGroupOp op) noexcept {
    using namespace std::string_view_literals;
    switch (op) {
        case ThreadGroupOp::SHADER_EXECUTION_REORDER:return "shader_execution_reorder"sv;
        case ThreadGroupOp::RASTER_QUAD_DDX:return "raster_quad_ddx"sv;
        case ThreadGroupOp::RASTER_QUAD_DDY:return "raster_quad_ddy"sv;
        case ThreadGroupOp::WARP_IS_FIRST_ACTIVE_LANE:return "warp_is_first_active_lane"sv;
        case ThreadGroupOp::WARP_FIRST_ACTIVE_LANE:return "warp_first_active_lane"sv;
        case ThreadGroupOp::WARP_ACTIVE_ALL_EQUAL:return "warp_active_all_equal"sv;
        case ThreadGroupOp::WARP_ACTIVE_BIT_AND:return "warp_active_bit_and"sv;
        case ThreadGroupOp::WARP_ACTIVE_BIT_OR:return "warp_active_bit_or"sv;
        case ThreadGroupOp::WARP_ACTIVE_BIT_XOR:return "warp_active_bit_xor"sv;
        case ThreadGroupOp::WARP_ACTIVE_COUNT_BITS:return "warp_active_count_bits"sv;
        case ThreadGroupOp::WARP_ACTIVE_MAX:return "warp_active_max"sv;
        case ThreadGroupOp::WARP_ACTIVE_MIN:return "warp_active_min"sv;
        case ThreadGroupOp::WARP_ACTIVE_PRODUCT:return "warp_active_product"sv;
        case ThreadGroupOp::WARP_ACTIVE_SUM:return "warp_active_sum"sv;
        case ThreadGroupOp::WARP_ACTIVE_ALL:return "warp_active_all"sv;
        case ThreadGroupOp::WARP_ACTIVE_ANY:return "warp_active_any"sv;
        case ThreadGroupOp::WARP_ACTIVE_BIT_MASK:return "warp_active_bit_mask"sv;
        case ThreadGroupOp::WARP_PREFIX_COUNT_BITS:return "warp_prefix_count_bits"sv;
        case ThreadGroupOp::WARP_PREFIX_SUM:return "warp_prefix_sum"sv;
        case ThreadGroupOp::WARP_PREFIX_PRODUCT:return "warp_prefix_product"sv;
        case ThreadGroupOp::WARP_READ_LANE:return "warp_read_lane"sv;
        case ThreadGroupOp::WARP_READ_FIRST_ACTIVE_LANE:return "warp_read_first_active_lane"sv;
        case ThreadGroupOp::SYNCHRONIZE_BLOCK:return "synchronize_block"sv;
    }
    return "unknown"sv;
}

class LC_XIR_API ThreadGroupInst final : public DerivedInstruction<DerivedInstructionTag::THREAD_GROUP> {

private:
    ThreadGroupOp _op;

public:
    explicit ThreadGroupInst(const Type *type = nullptr, ThreadGroupOp op = {},
                             luisa::span<Value *const> operands = {}) noexcept;
    [[nodiscard]] auto op() const noexcept { return _op; }
    void set_op(ThreadGroupOp op) noexcept { _op = op; }
};

}// namespace luisa::compute::xir
