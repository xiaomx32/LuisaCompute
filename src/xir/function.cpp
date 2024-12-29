#include "luisa/core/stl/unordered_map.h"

#include <luisa/ast/type.h>
#include <luisa/core/logging.h>
#include <luisa/xir/function.h>

namespace luisa::compute::xir {

Function::Function(const Type *type) noexcept : Super{type} {}

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
    if (type->is_resource()) {
        LUISA_ASSERT(!by_ref, "Resource argument must not be passed by reference.");
        return create_resource_argument(type);
    }
    return by_ref ? static_cast<Argument *>(create_reference_argument(type)) :
                    static_cast<Argument *>(create_value_argument(type));
}

ValueArgument *Function::create_value_argument(const Type *type) noexcept {
    LUISA_ASSERT(!type->is_resource(), "Resource argument must be created with create_resource_argument.");
    LUISA_ASSERT(!type->is_custom(), "Opaque argument must be created with create_reference_argument.");
    auto argument = Pool::current()->create<ValueArgument>(type, this);
    add_argument(argument);
    return argument;
}

ReferenceArgument *Function::create_reference_argument(const Type *type) noexcept {
    LUISA_ASSERT(!type->is_resource(), "Resource argument must be created with create_resource_argument.");
    auto argument = Pool::current()->create<ReferenceArgument>(type, this);
    add_argument(argument);
    return argument;
}

ResourceArgument *Function::create_resource_argument(const Type *type) noexcept {
    LUISA_ASSERT(type->is_resource(), "Resource argument must be created with create_resource_argument.");
    auto argument = Pool::current()->create<ResourceArgument>(type, this);
    add_argument(argument);
    return argument;
}

void FunctionDefinition::set_body_block(BasicBlock *block) noexcept {
    _body_block = block;
}

BasicBlock *FunctionDefinition::create_body_block(bool overwrite_existing) noexcept {
    LUISA_ASSERT(_body_block == nullptr || overwrite_existing,
                 "Body block already exists.");
    auto new_block = Pool::current()->create<BasicBlock>();
    set_body_block(new_block);
    return new_block;
}

namespace detail {

void traverse_basic_block_recursive(luisa::unordered_set<BasicBlock *> &visited, BasicBlock *block,
                                    void *visit_ctx, void (*visit)(void *, BasicBlock *)) noexcept {
    if (visited.emplace(block).second) {
        visit(visit_ctx, block);
        auto terminator = block->terminator();
        for (auto use : terminator->operand_uses()) {
            if (auto v = use->value();
                v != nullptr && v->derived_value_tag() == DerivedValueTag::BASIC_BLOCK) {
                traverse_basic_block_recursive(visited, static_cast<BasicBlock *>(v), visit_ctx, visit);
            }
        }
    }
}

}// namespace detail

void FunctionDefinition::_traverse_basic_block_recursive(BasicBlock *block, void *visit_ctx,
                                                         void (*visit)(void *, BasicBlock *)) noexcept {
    luisa::unordered_set<BasicBlock *> visited;
    detail::traverse_basic_block_recursive(visited, block, visit_ctx, visit);
}

KernelFunction::KernelFunction(luisa::uint3 block_size) noexcept {
    set_block_size(block_size);
}

void KernelFunction::set_block_size(luisa::uint3 size) noexcept {
    auto thread_count = size.x * size.y * size.z;
    LUISA_ASSERT(thread_count >= 32u &&
                     thread_count <= 1024u &&
                     thread_count % 32u == 0u,
                 "Invalid block size: {}.", size);
    _block_size = size;
}

}// namespace luisa::compute::xir
