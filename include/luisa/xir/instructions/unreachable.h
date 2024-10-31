#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

// Note: this instruction must be the terminator of a basic block.
class UnreachableInst final : public DerivedTerminatorInstruction<DerivedInstructionTag::UNREACHABLE> {
public:
    using DerivedTerminatorInstruction::DerivedTerminatorInstruction;
};

}// namespace luisa::compute::xir
