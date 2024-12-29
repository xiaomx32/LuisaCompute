#include <luisa/core/logging.h>
#include <luisa/xir/basic_block.h>
#include <luisa/xir/builder.h>

namespace luisa::compute::xir {

void Builder::_check_valid_insertion_point() const noexcept {
    LUISA_ASSERT(_insertion_point != nullptr, "Invalid insertion point.");
}

Builder::Builder() noexcept = default;

IfInst *Builder::if_(Value *cond) noexcept {
    LUISA_ASSERT(cond != nullptr && cond->type() == Type::of<bool>(), "Invalid condition.");
    return _create_and_append_instruction<IfInst>(cond);
}

SwitchInst *Builder::switch_(Value *value) noexcept {
    LUISA_ASSERT(value != nullptr, "Switch value cannot be null.");
    return _create_and_append_instruction<SwitchInst>(value);
}

LoopInst *Builder::loop() noexcept {
    return _create_and_append_instruction<LoopInst>();
}

SimpleLoopInst *Builder::simple_loop() noexcept {
    return _create_and_append_instruction<SimpleLoopInst>();
}

BranchInst *Builder::br(BasicBlock *target) noexcept {
    auto inst = _create_and_append_instruction<BranchInst>();
    inst->set_target_block(target);
    return inst;
}

ConditionalBranchInst *Builder::cond_br(Value *cond, BasicBlock *true_target, BasicBlock *false_target) noexcept {
    LUISA_ASSERT(cond != nullptr && cond->type() == Type::of<bool>(), "Invalid condition.");
    auto inst = _create_and_append_instruction<ConditionalBranchInst>(cond);
    inst->set_true_target(true_target);
    inst->set_false_target(false_target);
    return inst;
}

BreakInst *Builder::break_(BasicBlock *target_block) noexcept {
    auto inst = _create_and_append_instruction<BreakInst>();
    inst->set_target_block(target_block);
    return inst;
}

ContinueInst *Builder::continue_(BasicBlock *target_block) noexcept {
    auto inst = _create_and_append_instruction<ContinueInst>();
    inst->set_target_block(target_block);
    return inst;
}

UnreachableInst *Builder::unreachable_(luisa::string_view message) noexcept {
    return _create_and_append_instruction<UnreachableInst>(luisa::string{message});
}

AssertInst *Builder::assert_(Value *condition, luisa::string_view message) noexcept {
    return _create_and_append_instruction<AssertInst>(condition, luisa::string{message});
}

AssumeInst *Builder::assume_(Value *condition, luisa::string_view message) noexcept {
    return _create_and_append_instruction<AssumeInst>(condition, luisa::string{message});
}

ReturnInst *Builder::return_(Value *value) noexcept {
    return _create_and_append_instruction<ReturnInst>(value);
}

ReturnInst *Builder::return_void() noexcept {
    return _create_and_append_instruction<ReturnInst>();
}

RasterDiscardInst *Builder::raster_discard() noexcept {
    return _create_and_append_instruction<RasterDiscardInst>();
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
    auto inst = _create_and_append_instruction<PhiInst>(type);
    for (auto incoming : incomings) { inst->add_incoming(incoming.value, incoming.block); }
    return inst;
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

Instruction *Builder::static_cast_(const Type *type, Value *value) noexcept {
    LUISA_ASSERT(type->is_scalar() && value->type()->is_scalar(), "Invalid cast operation.");
    return _create_and_append_instruction<CastInst>(type, CastOp::STATIC_CAST, value);
}

CastInst *Builder::bit_cast_(const Type *type, Value *value) noexcept {
    return _create_and_append_instruction<CastInst>(type, CastOp::BITWISE_CAST, value);
}

Value *Builder::static_cast_if_necessary(const Type *type, Value *value) noexcept {
    return value->type() == type ? value : static_cast_(type, value);
}

Value *Builder::bit_cast_if_necessary(const Type *type, Value *value) noexcept {
    return value->type() == type ? value : bit_cast_(type, value);
}

PhiInst *Builder::phi(const Type *type, luisa::span<const PhiIncoming> incomings) noexcept {
    auto inst = _create_and_append_instruction<PhiInst>(type);
    for (auto incoming : incomings) { inst->add_incoming(incoming.value, incoming.block); }
    return inst;
}

PrintInst *Builder::print(luisa::string format, luisa::span<Value *const> values) noexcept {
    return _create_and_append_instruction<PrintInst>(std::move(format), values);
}

GEPInst *Builder::gep(const Type *type, Value *base, luisa::span<Value *const> indices) noexcept {
    return _create_and_append_instruction<GEPInst>(type, base, indices);
}

LoadInst *Builder::load(const Type *type, Value *variable) noexcept {
    LUISA_ASSERT(variable->is_lvalue(), "Load source must be an lvalue.");
    LUISA_ASSERT(type == variable->type(), "Type mismatch in Load");
    return _create_and_append_instruction<LoadInst>(type, variable);
}

StoreInst *Builder::store(Value *variable, Value *value) noexcept {
    LUISA_ASSERT(variable->is_lvalue(), "Store destination must be an lvalue.");
    LUISA_ASSERT(!value->is_lvalue(), "Store source cannot be an lvalue.");
    LUISA_ASSERT(variable->type() == value->type(), "Type mismatch in Store");
    return _create_and_append_instruction<StoreInst>(variable, value);
}

ClockInst *Builder::clock() noexcept {
    return _create_and_append_instruction<ClockInst>();
}

OutlineInst *Builder::outline() noexcept {
    return _create_and_append_instruction<OutlineInst>();
}

RayQueryInst *Builder::ray_query(Value *query_object) noexcept {
    return _create_and_append_instruction<RayQueryInst>(query_object);
}

ThreadGroupInst *Builder::call(const Type *type, ThreadGroupOp op, luisa::span<Value *const> operands) noexcept {
    return _create_and_append_instruction<ThreadGroupInst>(type, op, operands);
}

ThreadGroupInst *Builder::shader_execution_reorder() noexcept {
    return this->call(nullptr, ThreadGroupOp::SHADER_EXECUTION_REORDER, {});
}

ThreadGroupInst *Builder::shader_execution_reorder(Value *hint, Value *hint_bits) noexcept {
    return this->call(nullptr, ThreadGroupOp::SHADER_EXECUTION_REORDER, std::array{hint, hint_bits});
}

ThreadGroupInst *Builder::synchronize_block() noexcept {
    return this->call(nullptr, ThreadGroupOp::SYNCHRONIZE_BLOCK, {});
}

ThreadGroupInst *Builder::raster_quad_ddx(const Type *type, Value *value) noexcept {
    return this->call(type, ThreadGroupOp::RASTER_QUAD_DDX, std::array{value});
}

ThreadGroupInst *Builder::raster_quad_ddy(const Type *type, Value *value) noexcept {
    return this->call(type, ThreadGroupOp::RASTER_QUAD_DDY, std::array{value});
}

AtomicInst *Builder::call(const Type *type, AtomicOp op, Value *base, luisa::span<Value *const> indices, luisa::span<Value *const> values) noexcept {
    return _create_and_append_instruction<AtomicInst>(type, op, base, indices, values);
}

AtomicInst *Builder::atomic_fetch_add(const Type *type, Value *base,
                                      luisa::span<Value *const> indices,
                                      Value *value) noexcept {
    return this->call(type, AtomicOp::FETCH_ADD, base, indices, std::array{value});
}

AtomicInst *Builder::atomic_fetch_sub(const Type *type, Value *base,
                                      luisa::span<Value *const> indices,
                                      Value *value) noexcept {
    return this->call(type, AtomicOp::FETCH_SUB, base, indices, std::array{value});
}

AtomicInst *Builder::atomic_fetch_and(const Type *type, Value *base,
                                      luisa::span<Value *const> indices,
                                      Value *value) noexcept {
    return this->call(type, AtomicOp::FETCH_AND, base, indices, std::array{value});
}

AtomicInst *Builder::atomic_fetch_or(const Type *type, Value *base,
                                     luisa::span<Value *const> indices,
                                     Value *value) noexcept {
    return this->call(type, AtomicOp::FETCH_OR, base, indices, std::array{value});
}

AtomicInst *Builder::atomic_fetch_xor(const Type *type, Value *base,
                                      luisa::span<Value *const> indices,
                                      Value *value) noexcept {
    return this->call(type, AtomicOp::FETCH_XOR, base, indices, std::array{value});
}

AtomicInst *Builder::atomic_fetch_min(const Type *type, Value *base,
                                      luisa::span<Value *const> indices,
                                      Value *value) noexcept {
    return this->call(type, AtomicOp::FETCH_MIN, base, indices, std::array{value});
}

AtomicInst *Builder::atomic_fetch_max(const Type *type, Value *base,
                                      luisa::span<Value *const> indices,
                                      Value *value) noexcept {
    return this->call(type, AtomicOp::FETCH_MAX, base, indices, std::array{value});
}

AtomicInst *Builder::atomic_exchange(const Type *type, Value *base,
                                     luisa::span<Value *const> indices,
                                     Value *value) noexcept {
    return this->call(type, AtomicOp::EXCHANGE, base, indices, std::array{value});
}

AtomicInst *Builder::atomic_compare_exchange(const Type *type, Value *base,
                                             luisa::span<Value *const> indices,
                                             Value *expected, Value *desired) noexcept {
    return this->call(type, AtomicOp::COMPARE_EXCHANGE, base, indices, std::array{expected, desired});
}

void Builder::set_insertion_point(Instruction *insertion_point) noexcept {
    _insertion_point = insertion_point;
}

void Builder::set_insertion_point(BasicBlock *block) noexcept {
    auto instruction = block ? block->instructions().tail_sentinel()->prev() : nullptr;
    set_insertion_point(instruction);
}

}// namespace luisa::compute::xir
