#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

class BasicBlock;

class LC_XIR_API OutlineInst final : public DerivedInstruction<DerivedInstructionTag::OUTLINE> {

public:
    static constexpr size_t operand_index_body_block = 0u;
    static constexpr size_t operand_index_merge_block = 1u;

public:
    explicit OutlineInst(Pool *pool) noexcept;
    BasicBlock *create_body_block() noexcept;
    BasicBlock *create_merge_block() noexcept;
    void set_body_block(BasicBlock *body_block) noexcept;
    void set_merge_block(BasicBlock *merge_block) noexcept;
    [[nodiscard]] BasicBlock *body_block() noexcept;
    [[nodiscard]] BasicBlock *merge_block() noexcept;
    [[nodiscard]] const BasicBlock *body_block() const noexcept;
    [[nodiscard]] const BasicBlock *merge_block() const noexcept;
};

}// namespace luisa::compute::xir
