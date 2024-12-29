#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

class LC_XIR_API RasterDiscardInst final : public DerivedInstruction<DerivedInstructionTag::RASTER_DISCARD> {
public:
    using DerivedInstruction::DerivedInstruction;
};

}// namespace luisa::compute::xir
