#include <luisa/core/logging.h>
#include <luisa/xir/function.h>

namespace luisa::compute::xir {

Function::Function(FunctionTag tag, const Type *type) noexcept
    : Super{type}, _function_tag{tag} {}

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

Argument *Function::create_argument(const Type *type, bool by_ref) noexcept {
    return by_ref ? static_cast<Argument *>(create_reference_argument(type)) :
                    static_cast<Argument *>(create_value_argument(type));
}

ValueArgument *Function::create_value_argument(const Type *type) noexcept {
    auto argument = Pool::current()->create<ValueArgument>(type, this);
    add_argument(argument);
    return argument;
}

ReferenceArgument *Function::create_reference_argument(const Type *type) noexcept {
    auto argument = Pool::current()->create<ReferenceArgument>(type, this);
    add_argument(argument);
    return argument;
}

void Function::set_body_block(BasicBlock *block) noexcept {
    _body_block = block;
}

BasicBlock *Function::create_body_block(bool overwrite_existing) noexcept {
    LUISA_ASSERT(_body_block == nullptr || overwrite_existing,
                 "Body block already exists.");
    auto new_block = Pool::current()->create<BasicBlock>();
    set_body_block(new_block);
    return new_block;
}

}// namespace luisa::compute::xir
