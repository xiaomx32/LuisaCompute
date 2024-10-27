#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

class BasicBlock;

// Branch instruction:
//
// if (cond) {
//   true_block
// } else {
//   false_block
// }
// { merge_block }
//
// Note: this instruction must be the terminator of a basic block.
class LC_XIR_API BranchInst final : public Instruction {

public:
    static constexpr size_t operand_index_cond = 0u;
    static constexpr size_t operand_index_true_block = 1u;
    static constexpr size_t operand_index_false_block = 2u;
    static constexpr size_t operand_index_merge_block = 3u;

public:
    explicit BranchInst(Pool *pool, Value *cond = nullptr) noexcept;

    [[nodiscard]] DerivedInstructionTag derived_instruction_tag() const noexcept override {
        return DerivedInstructionTag::BRANCH;
    }

    void set_cond(Value *cond) noexcept;
    void set_true_block(BasicBlock *block) noexcept;
    void set_false_block(BasicBlock *block) noexcept;
    void set_merge_block(BasicBlock *block) noexcept;

    BasicBlock *create_true_block() noexcept;
    BasicBlock *create_false_block() noexcept;
    BasicBlock *create_merge_block() noexcept;

    [[nodiscard]] Value *cond() noexcept;
    [[nodiscard]] const Value *cond() const noexcept;

    [[nodiscard]] BasicBlock *true_block() noexcept;
    [[nodiscard]] const BasicBlock *true_block() const noexcept;

    [[nodiscard]] BasicBlock *false_block() noexcept;
    [[nodiscard]] const BasicBlock *false_block() const noexcept;

    [[nodiscard]] BasicBlock *merge_block() noexcept;
    [[nodiscard]] const BasicBlock *merge_block() const noexcept;
};

}// namespace luisa::compute::xir
