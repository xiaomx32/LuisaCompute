#include <luisa/xir/instructions/thread_group.h>

namespace luisa::compute::xir {

ThreadGroupInst::ThreadGroupInst(const Type *type, ThreadGroupOp op,
                                 luisa::span<Value *const> operands) noexcept
    : DerivedInstruction{type}, InstructionOpMixin{op} {
    if (!operands.empty()) {
        set_operands(operands);
    }
}

}// namespace luisa::compute::xir
