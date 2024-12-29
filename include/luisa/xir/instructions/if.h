#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

class BasicBlock;

// Branch instruction:
//
// if (cond) {
//   true_block
// } else {
//   false_block
// }
// { merge_block }
//
// Note: this instruction must be the terminator of a basic block.
class IfInst final : public ControlFlowMergeMixin<DerivedConditionalBranchInstruction<DerivedInstructionTag::IF>> {
public:
    using ControlFlowMergeMixin::ControlFlowMergeMixin;
};

}// namespace luisa::compute::xir
