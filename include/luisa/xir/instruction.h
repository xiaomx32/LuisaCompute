#pragma once

#include <luisa/xir/user.h>

namespace luisa::compute::xir {

class BasicBlock;

enum struct DerivedInstructionTag {

    /* utility instructions */
    SENTINEL,// sentinels in instruction list

    /* control flow instructions */
    UNREACHABLE,// basic block terminator: unreachable
    BRANCH,     // basic block terminator: conditional branches
    SWITCH,     // basic block terminator: switch branches
    LOOP,       // basic block terminator: loops
    BREAK,      // basic block terminator: break (removed after control flow normalization)
    CONTINUE,   // basic block terminator: continue (removed after control flow normalization)
    RETURN,     // basic block terminator: return (early returns are removed after control flow normalization)
    PHI,        // basic block beginning: phi nodes

    // variable instructions
    ALLOCA,
    LOAD,
    STORE,
    GEP,

    /* other instructions */
    CALL,     // user or external function calls
    INTRINSIC,// intrinsic function calls
    CAST,     // type casts
    PRINT,    // kernel print

    OUTLINE,  // mark that the body might be outlined (e.g., for faster compilation)
    AUTO_DIFF,// automatic differentiation
    RAY_QUERY,// ray queries
};

class LC_XIR_API Instruction : public IntrusiveNode<Instruction, DerivedValue<DerivedValueTag::INSTRUCTION, User>> {

private:
    friend BasicBlock;
    BasicBlock *_parent_block = nullptr;

protected:
    void _set_parent_block(BasicBlock *block) noexcept;
    void _remove_self_from_operand_use_lists() noexcept;
    void _add_self_to_operand_use_lists() noexcept;
    [[nodiscard]] bool _should_add_self_to_operand_use_lists() const noexcept override;

public:
    explicit Instruction(Pool *pool, const Type *type = nullptr) noexcept;
    [[nodiscard]] virtual DerivedInstructionTag derived_instruction_tag() const noexcept {
        return DerivedInstructionTag::SENTINEL;
    }

    void remove_self() noexcept override;
    void insert_before_self(Instruction *node) noexcept override;
    void insert_after_self(Instruction *node) noexcept override;
    void replace_self_with(Instruction *node) noexcept;

    [[nodiscard]] BasicBlock *parent_block() noexcept { return _parent_block; }
    [[nodiscard]] const BasicBlock *parent_block() const noexcept { return _parent_block; }
};

using InstructionList = InlineIntrusiveList<Instruction>;

template<DerivedInstructionTag tag, typename Base = Instruction>
class DerivedInstruction : public Base {
public:
    using Base::Base;

    [[nodiscard]] static constexpr DerivedInstructionTag
    static_derived_instruction_tag() noexcept { return tag; }

    [[nodiscard]] DerivedInstructionTag
    derived_instruction_tag() const noexcept final {
        return static_derived_instruction_tag();
    }
};

}// namespace luisa::compute::xir
