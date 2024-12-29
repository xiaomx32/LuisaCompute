#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

class LC_XIR_API ClockInst final : public DerivedInstruction<DerivedInstructionTag::CLOCK> {
public:
    ClockInst() noexcept;
};

}// namespace luisa::compute::xir
