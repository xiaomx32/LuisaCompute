#include <luisa/xir/instructions/continue.h>

namespace luisa::compute::xir {

ContinueInst::ContinueInst(Pool *pool) noexcept
    : DerivedInstruction{pool, nullptr} {}

}// namespace luisa::compute::xir
