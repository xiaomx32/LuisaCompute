#include <luisa/xir/instructions/arithmetic.h>

namespace luisa::compute::xir {

ArithmeticInst::ArithmeticInst(const Type *type, ArithmeticOp op,
                               luisa::span<Value *const> operands) noexcept
    : DerivedInstruction{type}, InstructionOpMixin{op} {
    set_operands(operands);
}

}// namespace luisa::compute::xir
