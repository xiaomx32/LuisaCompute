#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

class LC_XIR_API AssumeInst final : public DerivedInstruction<DerivedInstructionTag::ASSUME> {

public:
    static constexpr size_t operand_index_condition = 0u;

private:
    luisa::string _message;

public:
    explicit AssumeInst(Value *condition = nullptr,
                        luisa::string message = {}) noexcept;

    void set_condition(Value *condition) noexcept;
    [[nodiscard]] Value *condition() noexcept;
    [[nodiscard]] const Value *condition() const noexcept;

    void set_message(luisa::string_view message) noexcept;
    [[nodiscard]] luisa::string_view message() const noexcept;
};

}// namespace luisa::compute::xir

