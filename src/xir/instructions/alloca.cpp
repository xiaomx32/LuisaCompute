#include <luisa/xir/instructions/alloca.h>

namespace luisa::compute::xir {

AllocaInst::AllocaInst(Pool *pool, const Type *type, AllocSpace space) noexcept
    : DerivedInstruction{pool, type}, _space{space} {}

void AllocaInst::set_space(AllocSpace space) noexcept {
    _space = space;
}

}// namespace luisa::compute::xir
