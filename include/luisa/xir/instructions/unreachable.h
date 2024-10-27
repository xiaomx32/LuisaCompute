#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

// Note: this instruction must be the terminator of a basic block.
class LC_XIR_API UnreachableInst final : public DerivedInstruction<DerivedInstructionTag::UNREACHABLE> {
public:
    using DerivedInstruction::DerivedInstruction;
};

}// namespace luisa::compute::xir
