#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

class BasicBlock;

class LC_XIR_API OutlineInst final : public Instruction {

public:
    static constexpr size_t operand_index_body_block = 0u;
    static constexpr size_t operand_index_merge_block = 1u;

public:
    explicit OutlineInst(Pool *pool, const Name *name = nullptr) noexcept;
    [[nodiscard]] DerivedInstructionTag derived_instruction_tag() const noexcept override {
        return DerivedInstructionTag::OUTLINE;
    }
    BasicBlock *create_body_block(const Name *name = nullptr) noexcept;
    BasicBlock *create_merge_block(const Name *name = nullptr) noexcept;
    void set_body_block(BasicBlock *body_block) noexcept;
    void set_merge_block(BasicBlock *merge_block) noexcept;
    [[nodiscard]] BasicBlock *body_block() noexcept;
    [[nodiscard]] BasicBlock *merge_block() noexcept;
    [[nodiscard]] const BasicBlock *body_block() const noexcept;
    [[nodiscard]] const BasicBlock *merge_block() const noexcept;
};

}// namespace luisa::compute::xir
