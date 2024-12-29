#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

class RasterDiscardInst final : public DerivedInstruction<DerivedInstructionTag::RASTER_DISCARD> {
public:
    using DerivedInstruction::DerivedInstruction;
};

}// namespace luisa::compute::xir
