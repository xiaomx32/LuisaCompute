#include <luisa/core/logging.h>
#include <luisa/xir/basic_block.h>
#include <luisa/xir/instructions/ray_query.h>

namespace luisa::compute::xir {

RayQueryInst::RayQueryInst(Pool *pool, Value *query_object) noexcept
    : DerivedInstruction{pool, nullptr} {
    auto on_surface_candidate_block = static_cast<Value *>(nullptr);
    auto on_procedural_candidate_block = static_cast<Value *>(nullptr);
    auto merge_block = static_cast<Value *>(nullptr);
    auto operands = std::array{query_object, on_surface_candidate_block, on_procedural_candidate_block, merge_block};
    LUISA_DEBUG_ASSERT(operands[operand_index_query_object] == query_object, "Invalid query object operand.");
    set_operands(operands);
}

void RayQueryInst::set_query_object(Value *query_object) noexcept {
    set_operand(operand_index_query_object, query_object);
}

void RayQueryInst::set_on_surface_candidate_block(BasicBlock *block) noexcept {
    _replace_owned_basic_block(this->on_surface_candidate_block(), block);
    set_operand(operand_index_on_surface_candidate_block, block);
}

void RayQueryInst::set_on_procedural_candidate_block(BasicBlock *block) noexcept {
    _replace_owned_basic_block(this->on_procedural_candidate_block(), block);
    set_operand(operand_index_on_procedural_candidate_block, block);
}

void RayQueryInst::set_merge_block(BasicBlock *block) noexcept {
    _replace_owned_basic_block(this->merge_block(), block);
    set_operand(operand_index_merge_block, block);
}

BasicBlock *RayQueryInst::create_on_surface_candidate_block() noexcept {
    auto block = pool()->create<BasicBlock>();
    set_on_surface_candidate_block(block);
    return block;
}

BasicBlock *RayQueryInst::create_on_procedural_candidate_block() noexcept {
    auto block = pool()->create<BasicBlock>();
    set_on_procedural_candidate_block(block);
    return block;
}

BasicBlock *RayQueryInst::create_merge_block() noexcept {
    auto block = pool()->create<BasicBlock>();
    set_merge_block(block);
    return block;
}

Value *RayQueryInst::query_object() noexcept {
    return operand(operand_index_query_object);
}

BasicBlock *RayQueryInst::on_surface_candidate_block() noexcept {
    return static_cast<BasicBlock *>(operand(operand_index_on_surface_candidate_block));
}

BasicBlock *RayQueryInst::on_procedural_candidate_block() noexcept {
    return static_cast<BasicBlock *>(operand(operand_index_on_procedural_candidate_block));
}

BasicBlock *RayQueryInst::merge_block() noexcept {
    return static_cast<BasicBlock *>(operand(operand_index_merge_block));
}

const Value *RayQueryInst::query_object() const noexcept {
    return const_cast<RayQueryInst *>(this)->query_object();
}

const BasicBlock *RayQueryInst::on_surface_candidate_block() const noexcept {
    return const_cast<RayQueryInst *>(this)->on_surface_candidate_block();
}

const BasicBlock *RayQueryInst::on_procedural_candidate_block() const noexcept {
    return const_cast<RayQueryInst *>(this)->on_procedural_candidate_block();
}

const BasicBlock *RayQueryInst::merge_block() const noexcept {
    return const_cast<RayQueryInst *>(this)->merge_block();
}

}// namespace luisa::compute::xir
