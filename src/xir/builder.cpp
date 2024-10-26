#include <luisa/core/logging.h>
#include <luisa/xir/basic_block.h>
#include <luisa/xir/builder.h>

namespace luisa::compute::xir {

void Builder::_check_valid_insertion_point() const noexcept {
    LUISA_ASSERT(_insertion_point != nullptr, "Invalid insertion point.");
}

Pool *Builder::_pool_from_insertion_point() const noexcept {
    _check_valid_insertion_point();
    return _insertion_point->pool();
}

Builder::Builder() noexcept = default;

BranchInst *Builder::if_(Value *cond) noexcept {
    return _create_and_append_instruction<BranchInst>(cond);
}

SwitchInst *Builder::switch_(Value *value) noexcept {
    return _create_and_append_instruction<SwitchInst>(value);
}

LoopInst *Builder::loop() noexcept {
    return _create_and_append_instruction<LoopInst>();
}

BreakInst *Builder::break_() noexcept {
    return _create_and_append_instruction<BreakInst>();
}

ContinueInst *Builder::continue_() noexcept {
    return _create_and_append_instruction<ContinueInst>();
}

UnreachableInst *Builder::unreachable_() noexcept {
    return _create_and_append_instruction<UnreachableInst>();
}

ReturnInst *Builder::return_(Value *value) noexcept {
    return _create_and_append_instruction<ReturnInst>(value);
}

ReturnInst *Builder::return_void() noexcept {
    return _create_and_append_instruction<ReturnInst>();
}

CallInst *Builder::call(const Type *type, Value *callee, luisa::span<Value *const> arguments) noexcept {
    return _create_and_append_instruction<CallInst>(type, callee, arguments);
}

CallInst *Builder::call(const Type *type, Value *callee, std::initializer_list<Value *> arguments) noexcept {
    return _create_and_append_instruction<CallInst>(type, callee, luisa::span{arguments.begin(), arguments.end()});
}

IntrinsicInst *Builder::call(const Type *type, IntrinsicOp op, std::initializer_list<Value *> arguments) noexcept {
    return _create_and_append_instruction<IntrinsicInst>(type, op, luisa::span{arguments.begin(), arguments.end()});
}

PhiInst *Builder::phi(const Type *type, std::initializer_list<PhiIncoming> incomings) noexcept {
    return _create_and_append_instruction<PhiInst>(type, luisa::span{incomings.begin(), incomings.end()});
}

PrintInst *Builder::print(luisa::string format, std::initializer_list<Value *> values) noexcept {
    return _create_and_append_instruction<PrintInst>(std::move(format), luisa::span{values.begin(), values.end()});
}

AllocaInst *Builder::alloca_(const Type *type, AllocSpace space) noexcept {
    return _create_and_append_instruction<AllocaInst>(type, space);
}

AllocaInst *Builder::alloca_local(const Type *type) noexcept {
    return alloca_(type, AllocSpace::LOCAL);
}

AllocaInst *Builder::alloca_shared(const Type *type) noexcept {
    return alloca_(type, AllocSpace::SHARED);
}

GEPInst *Builder::gep(const Type *type, Value *base, std::initializer_list<Value *> indices) noexcept {
    return _create_and_append_instruction<GEPInst>(type, base, luisa::span{indices.begin(), indices.end()});
}

IntrinsicInst *Builder::call(const Type *type, IntrinsicOp op, luisa::span<Value *const> arguments) noexcept {
    return _create_and_append_instruction<IntrinsicInst>(type, op, arguments);
}

CastInst *Builder::static_cast_(const Type *type, Value *value) noexcept {
    return _create_and_append_instruction<CastInst>(type, CastOp::STATIC_CAST, value);
}

CastInst *Builder::bit_cast_(const Type *type, Value *value) noexcept {
    return _create_and_append_instruction<CastInst>(type, CastOp::BITWISE_CAST, value);
}

PhiInst *Builder::phi(const Type *type, luisa::span<const PhiIncoming> incomings) noexcept {
    return _create_and_append_instruction<PhiInst>(type, incomings);
}

PrintInst *Builder::print(luisa::string format, luisa::span<Value *const> values) noexcept {
    return _create_and_append_instruction<PrintInst>(std::move(format), values);
}

GEPInst *Builder::gep(const Type *type, Value *base, luisa::span<Value *const> indices) noexcept {
    return _create_and_append_instruction<GEPInst>(type, base, indices);
}

LoadInst *Builder::load(const Type *type, Value *variable) noexcept {
    return _create_and_append_instruction<LoadInst>(type, variable);
}

StoreInst *Builder::store(Value *variable, Value *value) noexcept {
    return _create_and_append_instruction<StoreInst>(variable, value);
}

CommentInst *Builder::comment(luisa::string text) noexcept {
    return _create_and_append_instruction<CommentInst>(std::move(text));
}

void Builder::set_insertion_point(Instruction *insertion_point) noexcept {
    _insertion_point = insertion_point;
}

void Builder::set_insertion_point(BasicBlock *block) noexcept {
    auto instruction = block ? block->instructions().tail_sentinel()->prev() : nullptr;
    set_insertion_point(instruction);
}

}// namespace luisa::compute::xir
