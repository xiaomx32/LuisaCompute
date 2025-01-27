//
// Created by Mike on 2024/10/20.
//

#include <luisa/core/logging.h>
#include <luisa/core/stl/unordered_map.h>
#include <luisa/xir/instructions/intrinsic.h>

namespace luisa::compute::xir {

IntrinsicInst::IntrinsicInst(const Type *type, IntrinsicOp op,
                             luisa::span<Value *const> operands) noexcept
    : DerivedInstruction{type}, InstructionOpMixin{op} { set_operands(operands); }

}// namespace luisa::compute::xir
