#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

class BranchInst : public DerivedBranchInstruction<DerivedInstructionTag::BRANCH> {
public:
    using DerivedBranchInstruction::DerivedBranchInstruction;
};

class ConditionalBranchInst : public DerivedConditionalBranchInstruction<DerivedInstructionTag::CONDITIONAL_BRANCH> {
public:
    using DerivedConditionalBranchInstruction::DerivedConditionalBranchInstruction;
};

}// namespace luisa::compute::xir
