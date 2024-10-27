#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

class Function;
class Instruction;
class User;

class LC_XIR_API BasicBlock : public DerivedValue<DerivedValueTag::BASIC_BLOCK> {

private:
    Value *_parent_value = nullptr;
    InstructionList _instructions;

private:
    friend class User;
    friend class Function;
    void _set_parent_value(Value *parent_value) noexcept;

public:
    explicit BasicBlock(Pool *pool) noexcept;
    [[nodiscard]] Value *parent_value() noexcept { return _parent_value; }
    [[nodiscard]] const Value *parent_value() const noexcept { return _parent_value; }
    [[nodiscard]] auto &instructions() noexcept { return _instructions; }
    [[nodiscard]] auto &instructions() const noexcept { return _instructions; }
};

}// namespace luisa::compute::xir
