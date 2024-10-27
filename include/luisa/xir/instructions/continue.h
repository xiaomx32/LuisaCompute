#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

// Note: this instruction must be the terminator of a basic block.
class LC_XIR_API ContinueInst final : public DerivedInstruction<DerivedInstructionTag::CONTINUE> {
public:
    explicit ContinueInst(Pool *pool) noexcept;
};

}// namespace luisa::compute::xir
