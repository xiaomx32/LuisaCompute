#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

class LC_XIR_API RayQueryInst final : public DerivedInstruction<DerivedInstructionTag::RAY_QUERY> {

public:
    static constexpr size_t operand_index_query_object = 0u;
    static constexpr size_t operand_index_on_surface_candidate_block = 1u;
    static constexpr size_t operand_index_on_procedural_candidate_block = 2u;
    static constexpr size_t operand_index_merge_block = 3u;

public:
    explicit RayQueryInst(Pool *pool, Value *query_object = nullptr) noexcept;

    void set_query_object(Value *query_object) noexcept;
    void set_on_surface_candidate_block(BasicBlock *block) noexcept;
    void set_on_procedural_candidate_block(BasicBlock *block) noexcept;
    void set_merge_block(BasicBlock *block) noexcept;

    BasicBlock *create_on_surface_candidate_block() noexcept;
    BasicBlock *create_on_procedural_candidate_block() noexcept;
    BasicBlock *create_merge_block() noexcept;

    [[nodiscard]] Value *query_object() noexcept;
    [[nodiscard]] BasicBlock *on_surface_candidate_block() noexcept;
    [[nodiscard]] BasicBlock *on_procedural_candidate_block() noexcept;
    [[nodiscard]] BasicBlock *merge_block() noexcept;
    [[nodiscard]] const Value *query_object() const noexcept;
    [[nodiscard]] const BasicBlock *on_surface_candidate_block() const noexcept;
    [[nodiscard]] const BasicBlock *on_procedural_candidate_block() const noexcept;
    [[nodiscard]] const BasicBlock *merge_block() const noexcept;
};

}// namespace luisa::compute::xir
