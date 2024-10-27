#include <luisa/xir/basic_block.h>
#include <luisa/xir/instructions/outline.h>

namespace luisa::compute::xir {

OutlineInst::OutlineInst(Pool *pool) noexcept
    : Instruction{pool, nullptr} {
    set_operand_count(2u);
}

void OutlineInst::set_body_block(BasicBlock *body_block) noexcept {
    _replace_owned_basic_block(this->body_block(), body_block);
    set_operand(operand_index_body_block, body_block);
}

void OutlineInst::set_merge_block(BasicBlock *merge_block) noexcept {
    _replace_owned_basic_block(this->merge_block(), merge_block);
    set_operand(operand_index_merge_block, merge_block);
}

BasicBlock *OutlineInst::body_block() noexcept {
    return static_cast<BasicBlock *>(operand(operand_index_body_block));
}

BasicBlock *OutlineInst::merge_block() noexcept {
    return static_cast<BasicBlock *>(operand(operand_index_merge_block));
}

const BasicBlock *OutlineInst::body_block() const noexcept {
    return const_cast<OutlineInst *>(this)->body_block();
}

const BasicBlock *OutlineInst::merge_block() const noexcept {
    return const_cast<OutlineInst *>(this)->merge_block();
}

BasicBlock *OutlineInst::create_body_block() noexcept {
    auto block = pool()->create<BasicBlock>();
    set_body_block(block);
    return block;
}

BasicBlock *OutlineInst::create_merge_block() noexcept {
    auto block = pool()->create<BasicBlock>();
    set_merge_block(block);
    return block;
}

}// namespace luisa::compute::xir
