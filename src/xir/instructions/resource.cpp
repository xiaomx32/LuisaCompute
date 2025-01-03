#include <luisa/xir/instructions/resource.h>

namespace luisa::compute::xir {

ResourceQueryInst::ResourceQueryInst(const Type *type, ResourceQueryOp op,
                                     luisa::span<Value *const> operands) noexcept
    : DerivedInstruction{type},
      InstructionOpMixin{op} { set_operands(operands); }

ResourceReadInst::ResourceReadInst(const Type *type, ResourceReadOp op,
                                   luisa::span<Value *const> operands) noexcept
    : DerivedInstruction{type},
      InstructionOpMixin{op} { set_operands(operands); }

ResourceWriteInst::ResourceWriteInst(ResourceWriteOp op,
                                     luisa::span<Value *const> operands) noexcept
    : DerivedInstruction{nullptr},
      InstructionOpMixin{op} { set_operands(operands); }

}// namespace luisa::compute::xir
