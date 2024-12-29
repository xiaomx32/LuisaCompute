#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

// Note: this instruction must be the terminator of a basic block.
class LC_XIR_API LoopInst final : public ControlFlowMergeMixin<DerivedTerminatorInstruction<DerivedInstructionTag::LOOP>> {

public:
    static constexpr size_t operand_index_prepare_block = 0u;

private:
    BasicBlock *_body_block{nullptr};
    BasicBlock *_update_block{nullptr};

public:
    LoopInst() noexcept;

    void set_prepare_block(BasicBlock *block) noexcept;
    void set_body_block(BasicBlock *block) noexcept;
    void set_update_block(BasicBlock *block) noexcept;

    BasicBlock *create_prepare_block(bool overwrite_existing = false) noexcept;
    BasicBlock *create_body_block(bool overwrite_existing = false) noexcept;
    BasicBlock *create_update_block(bool overwrite_existing = false) noexcept;

    [[nodiscard]] BasicBlock *prepare_block() noexcept;
    [[nodiscard]] const BasicBlock *prepare_block() const noexcept;

    [[nodiscard]] BasicBlock *body_block() noexcept;
    [[nodiscard]] const BasicBlock *body_block() const noexcept;

    [[nodiscard]] BasicBlock *update_block() noexcept;
    [[nodiscard]] const BasicBlock *update_block() const noexcept;
};

class LC_XIR_API SimpleLoopInst final : public ControlFlowMergeMixin<DerivedTerminatorInstruction<DerivedInstructionTag::SIMPLE_LOOP>> {
public:
    static constexpr size_t operand_index_body_block = 0u;

public:
    SimpleLoopInst() noexcept;
    void set_body_block(BasicBlock *block) noexcept;
    BasicBlock *create_body_block(bool overwrite_existing = false) noexcept;
    [[nodiscard]] BasicBlock *body_block() noexcept;
    [[nodiscard]] const BasicBlock *body_block() const noexcept;
};

}// namespace luisa::compute::xir
