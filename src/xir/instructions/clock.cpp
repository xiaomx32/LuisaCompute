#include <luisa/ast/type_registry.h>
#include <luisa/xir/instructions/clock.h>

namespace luisa::compute::xir {

ClockInst::ClockInst() noexcept
    : DerivedInstruction{Type::of<luisa::ulong>()} {}

}// namespace luisa::compute::xir
