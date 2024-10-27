#include <luisa/core/logging.h>
#include <luisa/xir/constant.h>
#include <luisa/xir/function.h>
#include <luisa/xir/module.h>

namespace luisa::compute::xir {

Module::Module() noexcept = default;

Function *Module::create_kernel() noexcept {
    auto f = pool()->create<Function>(FunctionTag::KERNEL, nullptr);
    add_function(f);
    return f;
}

Function *Module::create_callable(const Type *ret_type) noexcept {
    auto f = pool()->create<Function>(FunctionTag::CALLABLE, ret_type);
    add_function(f);
    return f;
}

Constant *Module::create_constant(const Type *type, const void *data) noexcept {
    return pool()->create<Constant>(type, data);
}

void Module::add_function(Function *function) noexcept {
    LUISA_DEBUG_ASSERT(function != nullptr && function->pool() == pool(),
                       "Function must be created from the same pool as the module.");
    LUISA_DEBUG_ASSERT(!function->is_linked(), "Function is already linked.");
    _functions.insert_front(function);
}

}// namespace luisa::compute::xir
