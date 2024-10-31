#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

class BasicBlock;

class LC_XIR_API OutlineInst final : public DerivedBranchInstruction<DerivedInstructionTag::OUTLINE>,
                                     public MergeInstructionMixin<OutlineInst> {
public:
    using DerivedBranchInstruction::DerivedBranchInstruction;
};

}// namespace luisa::compute::xir
