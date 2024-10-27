#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

// Note: this instruction must be the terminator of a basic block.
class LC_XIR_API BreakInst final : public DerivedInstruction<DerivedInstructionTag::BREAK> {
public:
    explicit BreakInst(Pool *pool) noexcept;
};

}// namespace luisa::compute::xir
