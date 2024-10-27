#include <luisa/xir/instructions/break.h>

namespace luisa::compute::xir {

BreakInst::BreakInst(Pool *pool) noexcept
    : Instruction{pool, nullptr} {}

}// namespace luisa::compute::xir
