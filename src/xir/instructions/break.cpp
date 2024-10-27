#include <luisa/xir/instructions/break.h>

namespace luisa::compute::xir {

BreakInst::BreakInst(Pool *pool) noexcept
    : DerivedInstruction{pool, nullptr} {}

}// namespace luisa::compute::xir
