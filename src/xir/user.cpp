#include <luisa/core/logging.h>
#include <luisa/xir/basic_block.h>
#include <luisa/xir/user.h>

namespace luisa::compute::xir {

void User::set_operand(size_t index, Value *value) noexcept {
    LUISA_DEBUG_ASSERT(index < _operands.size(), "Index out of range.");
    LUISA_DEBUG_ASSERT(_operands[index] != nullptr && _operands[index]->user() == this, "Invalid operand.");
    _operands[index]->set_value(value);
}

Use *User::operand_use(size_t index) noexcept {
    LUISA_DEBUG_ASSERT(index < _operands.size(), "Index out of range.");
    return _operands[index];
}

const Use *User::operand_use(size_t index) const noexcept {
    LUISA_DEBUG_ASSERT(index < _operands.size(), "Index out of range.");
    return _operands[index];
}

Value *User::operand(size_t index) noexcept {
    return operand_use(index)->value();
}

const Value *User::operand(size_t index) const noexcept {
    return operand_use(index)->value();
}

void User::_replace_owned_basic_block(BasicBlock *old_block, BasicBlock *new_block) noexcept {
    if (old_block) { old_block->_set_parent_value(nullptr); }
    if (new_block) { new_block->_set_parent_value(this); }
}

void User::set_operand_count(size_t n) noexcept {
    if (n < _operands.size()) {// remove redundant operands
        for (auto i = n; i < _operands.size(); i++) {
            operand_use(i)->set_value(nullptr);
        }
        _operands.resize(n);
    } else {// create new operands
        _operands.reserve(n);
        for (auto i = _operands.size(); i < n; i++) {
            auto use = pool()->create<Use>(this, nullptr);
            _operands.emplace_back(use);
        }
    }
}

void User::set_operands(luisa::span<Value *const> operands) noexcept {
    set_operand_count(operands.size());
    for (auto i = 0u; i < operands.size(); i++) {
        set_operand(i, operands[i]);
    }
}

void User::add_operand(Value *value) noexcept {
    auto use = pool()->create<Use>(this, value);
    _operands.emplace_back(use);
}

void User::insert_operand(size_t index, Value *value) noexcept {
    LUISA_DEBUG_ASSERT(index <= _operands.size(), "Index out of range.");
    auto use = pool()->create<Use>(this, value);
    _operands.insert(_operands.cbegin() + index, use);
}

void User::remove_operand(size_t index) noexcept {
    if (index < _operands.size()) {
        _operands[index]->set_value(nullptr);
        _operands.erase(_operands.cbegin() + index);
    }
}

luisa::span<Use *> User::operand_uses() noexcept {
    return _operands;
}

luisa::span<const Use *const> User::operand_uses() const noexcept {
    return _operands;
}

size_t User::operand_count() const noexcept {
    return _operands.size();
}

}// namespace luisa::compute::xir
