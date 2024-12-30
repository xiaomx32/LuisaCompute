#pragma once

#include <luisa/ast/type_registry.h>
#include <luisa/xir/constant.h>
#include <luisa/xir/instructions/alloca.h>
#include <luisa/xir/instructions/arithmetic.h>
#include <luisa/xir/instructions/assert.h>
#include <luisa/xir/instructions/assume.h>
#include <luisa/xir/instructions/atomic.h>
#include <luisa/xir/instructions/branch.h>
#include <luisa/xir/instructions/break.h>
#include <luisa/xir/instructions/call.h>
#include <luisa/xir/instructions/cast.h>
#include <luisa/xir/instructions/clock.h>
#include <luisa/xir/instructions/continue.h>
#include <luisa/xir/instructions/gep.h>
#include <luisa/xir/instructions/if.h>
#include <luisa/xir/instructions/intrinsic.h>
#include <luisa/xir/instructions/load.h>
#include <luisa/xir/instructions/loop.h>
#include <luisa/xir/instructions/outline.h>
#include <luisa/xir/instructions/phi.h>
#include <luisa/xir/instructions/print.h>
#include <luisa/xir/instructions/ray_query.h>
#include <luisa/xir/instructions/raster_discard.h>
#include <luisa/xir/instructions/return.h>
#include <luisa/xir/instructions/resource_query.h>
#include <luisa/xir/instructions/resource_read.h>
#include <luisa/xir/instructions/resource_write.h>
#include <luisa/xir/instructions/store.h>
#include <luisa/xir/instructions/switch.h>
#include <luisa/xir/instructions/thread_group.h>
#include <luisa/xir/instructions/unreachable.h>

namespace luisa::compute::xir {
class ClockInst;
}// namespace luisa::compute::xir
namespace luisa::compute::xir {

class LC_XIR_API Builder {

private:
    Instruction *_insertion_point = nullptr;

private:
    void _check_valid_insertion_point() const noexcept;

    template<typename T, typename... Args>
    [[nodiscard]] auto _create_and_append_instruction(Args &&...args) noexcept {
        auto inst = Pool::current()->create<T>(std::forward<Args>(args)...);
        _insertion_point->insert_after_self(inst);
        set_insertion_point(inst);
        return inst;
    }

public:
    Builder() noexcept;
    void set_insertion_point(Instruction *insertion_point) noexcept;
    void set_insertion_point(BasicBlock *block) noexcept;
    [[nodiscard]] auto insertion_point() noexcept -> Instruction * { return _insertion_point; }
    [[nodiscard]] auto insertion_point() const noexcept -> const Instruction * { return _insertion_point; }

    [[nodiscard]] auto is_insertion_point_terminator() const noexcept {
        return _insertion_point != nullptr &&
               _insertion_point->is_terminator();
    }

public:
    IfInst *if_(Value *cond) noexcept;
    SwitchInst *switch_(Value *value) noexcept;
    LoopInst *loop() noexcept;
    SimpleLoopInst *simple_loop() noexcept;

    BranchInst *br(BasicBlock *target = nullptr) noexcept;
    ConditionalBranchInst *cond_br(Value *cond, BasicBlock *true_target = nullptr, BasicBlock *false_target = nullptr) noexcept;

    BreakInst *break_(BasicBlock *target_block = nullptr) noexcept;
    ContinueInst *continue_(BasicBlock *target_block = nullptr) noexcept;
    UnreachableInst *unreachable_(luisa::string_view message = {}) noexcept;
    ReturnInst *return_(Value *value) noexcept;
    ReturnInst *return_void() noexcept;
    RasterDiscardInst *raster_discard() noexcept;

    AssertInst *assert_(Value *condition, luisa::string_view message = {}) noexcept;
    AssumeInst *assume_(Value *condition, luisa::string_view message = {}) noexcept;

    CallInst *call(const Type *type, Value *callee, luisa::span<Value *const> arguments) noexcept;
    CallInst *call(const Type *type, Value *callee, std::initializer_list<Value *> arguments) noexcept;

    IntrinsicInst *call(const Type *type, IntrinsicOp op, luisa::span<Value *const> arguments) noexcept;
    IntrinsicInst *call(const Type *type, IntrinsicOp op, std::initializer_list<Value *> arguments) noexcept;

    AtomicInst *call(const Type *type, AtomicOp op, Value *base, luisa::span<Value *const> indices, luisa::span<Value *const> values) noexcept;
    AtomicInst *call(const Type *type, AtomicOp op, Value *base, luisa::span<Value *const> indices, std::initializer_list<Value *> values) noexcept;

    ThreadGroupInst *call(const Type *type, ThreadGroupOp op, luisa::span<Value *const> operands) noexcept;
    ThreadGroupInst *call(const Type *type, ThreadGroupOp op, std::initializer_list<Value *> operands) noexcept;

    ArithmeticInst *call(const Type *type, ArithmeticOp op, luisa::span<Value *const> operands) noexcept;
    ArithmeticInst *call(const Type *type, ArithmeticOp op, std::initializer_list<Value *> operands) noexcept;

    ResourceQueryInst *call(const Type *type, ResourceQueryOp op, luisa::span<Value *const> operands) noexcept;
    ResourceQueryInst *call(const Type *type, ResourceQueryOp op, std::initializer_list<Value *> operands) noexcept;

    ResourceReadInst *call(const Type *type, ResourceReadOp op, luisa::span<Value *const> operands) noexcept;
    ResourceReadInst *call(const Type *type, ResourceReadOp op, std::initializer_list<Value *> operands) noexcept;

    ResourceWriteInst *call(ResourceWriteOp op, luisa::span<Value *const> operands) noexcept;
    ResourceWriteInst *call(ResourceWriteOp op, std::initializer_list<Value *> operands) noexcept;

    Instruction *static_cast_(const Type *type, Value *value) noexcept;
    CastInst *bit_cast_(const Type *type, Value *value) noexcept;

    Value *static_cast_if_necessary(const Type *type, Value *value) noexcept;
    Value *bit_cast_if_necessary(const Type *type, Value *value) noexcept;

    PhiInst *phi(const Type *type, luisa::span<const PhiIncoming> incomings = {}) noexcept;
    PhiInst *phi(const Type *type, std::initializer_list<PhiIncoming> incomings) noexcept;

    PrintInst *print(luisa::string format, luisa::span<Value *const> values) noexcept;
    PrintInst *print(luisa::string format, std::initializer_list<Value *> values) noexcept;

    AllocaInst *alloca_(const Type *type, AllocSpace space) noexcept;
    AllocaInst *alloca_local(const Type *type) noexcept;
    AllocaInst *alloca_shared(const Type *type) noexcept;

    GEPInst *gep(const Type *type, Value *base, luisa::span<Value *const> indices) noexcept;
    GEPInst *gep(const Type *type, Value *base, std::initializer_list<Value *> indices) noexcept;

    LoadInst *load(const Type *type, Value *variable) noexcept;
    StoreInst *store(Value *variable, Value *value) noexcept;

    ClockInst *clock() noexcept;

    OutlineInst *outline() noexcept;
    RayQueryInst *ray_query(Value *query_object) noexcept;

    AtomicInst *atomic_fetch_add(const Type *type, Value *base, luisa::span<Value *const> indices, Value *value) noexcept;
    AtomicInst *atomic_fetch_sub(const Type *type, Value *base, luisa::span<Value *const> indices, Value *value) noexcept;
    AtomicInst *atomic_fetch_and(const Type *type, Value *base, luisa::span<Value *const> indices, Value *value) noexcept;
    AtomicInst *atomic_fetch_or(const Type *type, Value *base, luisa::span<Value *const> indices, Value *value) noexcept;
    AtomicInst *atomic_fetch_xor(const Type *type, Value *base, luisa::span<Value *const> indices, Value *value) noexcept;
    AtomicInst *atomic_fetch_min(const Type *type, Value *base, luisa::span<Value *const> indices, Value *value) noexcept;
    AtomicInst *atomic_fetch_max(const Type *type, Value *base, luisa::span<Value *const> indices, Value *value) noexcept;
    AtomicInst *atomic_exchange(const Type *type, Value *base, luisa::span<Value *const> indices, Value *value) noexcept;
    AtomicInst *atomic_compare_exchange(const Type *type, Value *base, luisa::span<Value *const> indices, Value *expected, Value *desired) noexcept;

    ThreadGroupInst *shader_execution_reorder() noexcept;
    ThreadGroupInst *shader_execution_reorder(Value *hint, Value *hint_bits) noexcept;
    ThreadGroupInst *synchronize_block() noexcept;
    ThreadGroupInst *raster_quad_ddx(const Type *type, Value *value) noexcept;
    ThreadGroupInst *raster_quad_ddy(const Type *type, Value *value) noexcept;
};

}// namespace luisa::compute::xir
