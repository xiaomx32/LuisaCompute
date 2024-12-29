#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

class BasicBlock;

class OutlineInst final : public ControlFlowMergeMixin<DerivedBranchInstruction<DerivedInstructionTag::OUTLINE>> {
public:
    using ControlFlowMergeMixin::ControlFlowMergeMixin;
};

}// namespace luisa::compute::xir
