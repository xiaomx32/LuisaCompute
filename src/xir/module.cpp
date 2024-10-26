#include "luisa/core/logging.h"

#include <luisa/xir/function.h>
#include <luisa/xir/module.h>

namespace luisa::compute::xir {

Module::Module(const Name *name) noexcept : _name{name} {}

Function *Module::create_kernel(const Name *name) noexcept {
    auto f = pool()->create<Function>(FunctionTag::KERNEL, nullptr, name);
    add_function(f);
    return f;
}

Function *Module::create_callable(const Type *ret_type, const Name *name) noexcept {
    auto f = pool()->create<Function>(FunctionTag::CALLABLE, ret_type, name);
    add_function(f);
    return f;
}

void Module::set_name(const Name *name) noexcept {
    _name = name;
}

void Module::add_function(Function *function) noexcept {
    LUISA_DEBUG_ASSERT(function != nullptr && function->pool() == pool(),
                       "Function must be created from the same pool as the module.");
    LUISA_DEBUG_ASSERT(!function->is_linked(), "Function is already linked.");
    _functions.insert_front(function);
}

}// namespace luisa::compute::xir
