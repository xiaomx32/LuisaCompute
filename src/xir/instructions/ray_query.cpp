#include <luisa/core/logging.h>
#include <luisa/xir/basic_block.h>
#include <luisa/xir/instructions/ray_query.h>

namespace luisa::compute::xir {

RayQueryObjectReadInst::RayQueryObjectReadInst(const Type *type, RayQueryObjectReadOp op,
                                               luisa::span<Value *const> operands) noexcept
    : DerivedInstruction{type},
      InstructionOpMixin{op} { set_operands(operands); }

RayQueryObjectWriteInst::RayQueryObjectWriteInst(RayQueryObjectWriteOp op,
                                                 luisa::span<Value *const> operands) noexcept
    : DerivedInstruction{nullptr},
      InstructionOpMixin{op} { set_operands(operands); }

RayQueryLoopInst::RayQueryLoopInst() noexcept {
    auto dispatch_block = static_cast<Value *>(nullptr);
    auto operands = std::array{dispatch_block};
    set_operands(operands);
}

void RayQueryLoopInst::set_dispatch_block(BasicBlock *block) noexcept {
    set_operand(operand_index_dispatch_block, block);
}

BasicBlock *RayQueryLoopInst::create_dispatch_block() noexcept {
    auto block = Pool::current()->create<BasicBlock>();
    set_dispatch_block(block);
    return block;
}

BasicBlock *RayQueryLoopInst::dispatch_block() noexcept {
    return static_cast<BasicBlock *>(operand(operand_index_dispatch_block));
}

const BasicBlock *RayQueryLoopInst::dispatch_block() const noexcept {
    return const_cast<RayQueryLoopInst *>(this)->dispatch_block();
}

RayQueryDispatchInst::RayQueryDispatchInst(Value *query_object) noexcept {
    auto exit_block = static_cast<Value *>(nullptr);
    auto on_surface_candidate_block = static_cast<Value *>(nullptr);
    auto on_procedural_candidate_block = static_cast<Value *>(nullptr);
    auto operands = std::array{query_object, exit_block, on_surface_candidate_block, on_procedural_candidate_block};
    LUISA_DEBUG_ASSERT(operands[operand_index_query_object] == query_object, "Invalid query object operand.");
    set_operands(operands);
}

void RayQueryDispatchInst::set_query_object(Value *query_object) noexcept {
    set_operand(operand_index_query_object, query_object);
}

void RayQueryDispatchInst::set_exit_block(BasicBlock *block) noexcept {
    set_operand(operand_index_exit_block, block);
}

void RayQueryDispatchInst::set_on_surface_candidate_block(BasicBlock *block) noexcept {
    set_operand(operand_index_on_surface_candidate_block, block);
}

void RayQueryDispatchInst::set_on_procedural_candidate_block(BasicBlock *block) noexcept {
    set_operand(operand_index_on_procedural_candidate_block, block);
}

BasicBlock *RayQueryDispatchInst::create_on_surface_candidate_block() noexcept {
    auto block = Pool::current()->create<BasicBlock>();
    set_on_surface_candidate_block(block);
    return block;
}

BasicBlock *RayQueryDispatchInst::create_on_procedural_candidate_block() noexcept {
    auto block = Pool::current()->create<BasicBlock>();
    set_on_procedural_candidate_block(block);
    return block;
}

Value *RayQueryDispatchInst::query_object() noexcept {
    return operand(operand_index_query_object);
}

const Value *RayQueryDispatchInst::query_object() const noexcept {
    return operand(operand_index_query_object);
}

BasicBlock *RayQueryDispatchInst::exit_block() noexcept {
    return static_cast<BasicBlock *>(operand(operand_index_exit_block));
}

const BasicBlock *RayQueryDispatchInst::exit_block() const noexcept {
    return const_cast<RayQueryDispatchInst *>(this)->exit_block();
}

BasicBlock *RayQueryDispatchInst::on_surface_candidate_block() noexcept {
    return static_cast<BasicBlock *>(operand(operand_index_on_surface_candidate_block));
}

const BasicBlock *RayQueryDispatchInst::on_surface_candidate_block() const noexcept {
    return const_cast<RayQueryDispatchInst *>(this)->on_surface_candidate_block();
}

BasicBlock *RayQueryDispatchInst::on_procedural_candidate_block() noexcept {
    return static_cast<BasicBlock *>(operand(operand_index_on_procedural_candidate_block));
}

const BasicBlock *RayQueryDispatchInst::on_procedural_candidate_block() const noexcept {
    return const_cast<RayQueryDispatchInst *>(this)->on_procedural_candidate_block();
}

}// namespace luisa::compute::xir
