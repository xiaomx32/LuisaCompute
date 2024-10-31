#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

// Note: this instruction must be the terminator of a basic block.
class BreakInst final : public DerivedTerminatorInstruction<DerivedInstructionTag::BREAK> {
public:
    using DerivedTerminatorInstruction::DerivedTerminatorInstruction;
};

}// namespace luisa::compute::xir
