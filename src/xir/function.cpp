#include <luisa/core/logging.h>
#include <luisa/xir/function.h>

namespace luisa::compute::xir {

Function::Function(Pool *pool, FunctionTag tag, const Type *type, const Name *name) noexcept
    : Super{pool, type, name},
      _body{pool->create<BasicBlock>()},
      _function_tag{tag} { _body->_set_parent_value(this); }

void Function::add_argument(Argument *argument) noexcept {
    argument->_set_parent_function(this);
    _arguments.emplace_back(argument);
}

void Function::insert_argument(size_t index, Argument *argument) noexcept {
    argument->_set_parent_function(this);
    _arguments.insert(_arguments.begin() + index, argument);
}

void Function::remove_argument(Argument *argument) noexcept {
    for (auto i = 0u; i < _arguments.size(); i++) {
        if (_arguments[i] == argument) {
            remove_argument(i);
            return;
        }
    }
    LUISA_ERROR_WITH_LOCATION("Argument not found.");
}

void Function::remove_argument(size_t index) noexcept {
    LUISA_ASSERT(index < _arguments.size(), "Argument index out of range.");
    _arguments[index]->_set_parent_function(nullptr);
    _arguments.erase(_arguments.begin() + index);
}

void Function::replace_argument(Argument *old_argument, Argument *new_argument) noexcept {
    if (old_argument != new_argument) {
        for (auto i = 0u; i < _arguments.size(); i++) {
            if (_arguments[i] == old_argument) {
                replace_argument(i, new_argument);
                return;
            }
        }
        LUISA_ERROR_WITH_LOCATION("Argument not found.");
    }
}

void Function::replace_argument(size_t index, Argument *argument) noexcept {
    LUISA_ASSERT(index < _arguments.size(), "Argument index out of range.");
    _arguments[index]->_set_parent_function(nullptr);
    _arguments[index]->replace_all_uses_with(argument);
    argument->_set_parent_function(this);
    _arguments[index] = argument;
}

Argument *Function::create_argument(const Type *type, bool by_ref, const Name *name) noexcept {
    return by_ref ? static_cast<Argument *>(create_reference_argument(type, name)) :
                    static_cast<Argument *>(create_value_argument(type, name));
}

ValueArgument *Function::create_value_argument(const Type *type, const Name *name) noexcept {
    auto argument = pool()->create<ValueArgument>(type, this, name);
    add_argument(argument);
    return argument;
}

ReferenceArgument *Function::create_reference_argument(const Type *type, const Name *name) noexcept {
    auto argument = pool()->create<ReferenceArgument>(type, this, name);
    add_argument(argument);
    return argument;
}

}// namespace luisa::compute::xir
