#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

enum struct IntrinsicOp {

    // no-op placeholder
    NOP,

    // autodiff ops
    AUTODIFF_REQUIRES_GRADIENT,  // (expr) -> void
    AUTODIFF_GRADIENT,           // (expr) -> expr
    AUTODIFF_GRADIENT_MARKER,    // (ref, expr) -> void
    AUTODIFF_ACCUMULATE_GRADIENT,// (ref, expr) -> void
    AUTODIFF_BACKWARD,           // (expr) -> void
    AUTODIFF_DETACH,             // (expr) -> expr

    // ray query
    RAY_QUERY_WORLD_SPACE_RAY,         // (RayQuery): Ray
    RAY_QUERY_PROCEDURAL_CANDIDATE_HIT,// (RayQuery): ProceduralHit
    RAY_QUERY_TRIANGLE_CANDIDATE_HIT,  // (RayQuery): TriangleHit
    RAY_QUERY_COMMITTED_HIT,           // (RayQuery): CommittedHit
    RAY_QUERY_COMMIT_TRIANGLE,         // (RayQuery): void
    RAY_QUERY_COMMIT_PROCEDURAL,       // (RayQuery, float): void
    RAY_QUERY_TERMINATE,               // (RayQuery): void

    // ray query extensions for backends with native support
    RAY_QUERY_PROCEED,
    RAY_QUERY_IS_TRIANGLE_CANDIDATE,
    RAY_QUERY_IS_PROCEDURAL_CANDIDATE,

};

[[nodiscard]] LC_XIR_API luisa::string_view to_string(IntrinsicOp op) noexcept;
[[nodiscard]] LC_XIR_API IntrinsicOp intrinsic_op_from_string(luisa::string_view name) noexcept;

class LC_XIR_API IntrinsicInst final : public DerivedInstruction<DerivedInstructionTag::INTRINSIC>,
                                       public InstructionOpMixin<IntrinsicOp> {
public:
    explicit IntrinsicInst(const Type *type = nullptr,
                           IntrinsicOp op = IntrinsicOp::NOP,
                           luisa::span<Value *const> operands = {}) noexcept;
};

}// namespace luisa::compute::xir
