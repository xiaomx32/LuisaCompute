#include <luisa/xir/argument.h>

namespace luisa::compute::xir {

Argument::Argument(Pool *pool, const Type *type,
                   Function *parent_function,
                   const Name *name) noexcept
    : Value{pool, type, name},
      _parent_function{parent_function} {}

void Argument::_set_parent_function(Function *func) noexcept {
    _parent_function = func;
}

}// namespace luisa::compute::xir
