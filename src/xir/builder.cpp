#include <luisa/core/logging.h>
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

IntrinsicInst *Builder::call(const Type *type, IntrinsicOp op, luisa::span<Value *const> arguments) noexcept {
    return _create_and_append_instruction<IntrinsicInst>(type, op, arguments);
}

CastInst *Builder::static_cast_(const Type *type, Value *value) noexcept {
    return _create_and_append_instruction<CastInst>(type, CastOp::STATIC_CAST, value);
}

CastInst *Builder::bitwise_cast_(const Type *type, Value *value) noexcept {
    return _create_and_append_instruction<CastInst>(type, CastOp::BITWISE_CAST, value);
}

PhiInst *Builder::phi(const Type *type) noexcept {
    return _create_and_append_instruction<PhiInst>(type);
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
