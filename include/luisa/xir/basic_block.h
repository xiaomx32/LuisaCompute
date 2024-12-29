#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

class Function;
class Instruction;
class User;

class LC_XIR_API BasicBlock : public DerivedValue<DerivedValueTag::BASIC_BLOCK> {

private:
    InstructionList _instructions;

public:
    BasicBlock() noexcept;
    [[nodiscard]] auto &instructions() noexcept { return _instructions; }
    [[nodiscard]] auto &instructions() const noexcept { return _instructions; }

    [[nodiscard]] bool is_terminated() const noexcept;
    [[nodiscard]] TerminatorInstruction *terminator() noexcept;
    [[nodiscard]] const TerminatorInstruction *terminator() const noexcept;

    template<typename Visit>
    void traverse_instructions(Visit &&visit) noexcept {
        for (auto &inst : _instructions) {
            visit(&inst);
        }
    }
    template<typename Visit>
    void traverse_instructions(Visit &&visit) const noexcept {
        for (auto &inst : _instructions) {
            visit(&inst);
        }
    }
};

}// namespace luisa::compute::xir
