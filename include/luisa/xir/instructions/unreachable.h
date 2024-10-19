#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

// Note: this instruction must be the terminator of a basic block.
class LC_XIR_API UnreachableInst : public Instruction {
public:
    using Instruction::Instruction;
    [[nodiscard]] DerivedInstructionTag derived_instruction_tag() const noexcept final {
        return DerivedInstructionTag::UNREACHABLE;
    }
};

}// namespace luisa::compute::xir