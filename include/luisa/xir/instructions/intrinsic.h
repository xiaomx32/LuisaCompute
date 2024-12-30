#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

enum struct IntrinsicOp {

    // no-op placeholder
    NOP,

    // resource operations
    BUFFER_READ, /// [(buffer, index) -> value]: reads the index-th element in buffer
    BUFFER_WRITE,/// [(buffer, index, value) -> void]: writes value into the index-th element of buffer
    BUFFER_SIZE, /// [(buffer) -> size]

    BYTE_BUFFER_READ, /// [(buffer, byte_index) -> value]: reads the index-th element in buffer
    BYTE_BUFFER_WRITE,/// [(buffer, byte_index, value) -> void]: writes value into the index-th element of buffer
    BYTE_BUFFER_SIZE, /// [(buffer) -> size_bytes]

    TEXTURE2D_READ,             /// [(texture, coord) -> value]
    TEXTURE2D_WRITE,            /// [(texture, coord, value) -> void]
    TEXTURE2D_SIZE,             /// [(texture) -> Vector<uint, dim>]
    TEXTURE2D_SAMPLE,           // (tex, uv: float2, filter: uint, level: uint): float4
    TEXTURE2D_SAMPLE_LEVEL,     // (tex, uv: float2, level: float, filter: uint, level: uint): float4
    TEXTURE2D_SAMPLE_GRAD,      // (tex, uv: float2, ddx: float2, ddy: float2, filter: uint, level: uint): float4
    TEXTURE2D_SAMPLE_GRAD_LEVEL,// (tex, uv: float2, ddx: float2, ddy: float2,  mip_clamp: float, filter: uint, level: uint): float4

    TEXTURE3D_READ,             /// [(texture, coord) -> value]
    TEXTURE3D_WRITE,            /// [(texture, coord, value) -> void]
    TEXTURE3D_SIZE,             /// [(texture) -> Vector<uint, dim>]
    TEXTURE3D_SAMPLE,           // (tex, uv: float3, filter: uint, level: uint): float4
    TEXTURE3D_SAMPLE_LEVEL,     // (tex, uv: float3, level: float, filter: uint, level: uint): float4
    TEXTURE3D_SAMPLE_GRAD,      // (tex, uv: float3, ddx: float3, ddy: float3, filter: uint, level: uint): float4
    TEXTURE3D_SAMPLE_GRAD_LEVEL,// (tex, uv: float3, ddx: float3, ddy: float3,  mip_clamp: float, filter: uint, level: uint): float4

    // bindless array operations
    BINDLESS_TEXTURE2D_SAMPLE,           // (bindless_array, index: uint, uv: float2): float4
    BINDLESS_TEXTURE2D_SAMPLE_LEVEL,     // (bindless_array, index: uint, uv: float2, level: float): float4
    BINDLESS_TEXTURE2D_SAMPLE_GRAD,      // (bindless_array, index: uint, uv: float2, ddx: float2, ddy: float2): float4
    BINDLESS_TEXTURE2D_SAMPLE_GRAD_LEVEL,// (bindless_array, index: uint, uv: float2, ddx: float2, ddy: float2, mip_clamp: float): float4
    BINDLESS_TEXTURE3D_SAMPLE,           // (bindless_array, index: uint, uv: float3): float4
    BINDLESS_TEXTURE3D_SAMPLE_LEVEL,     // (bindless_array, index: uint, uv: float3, level: float): float4
    BINDLESS_TEXTURE3D_SAMPLE_GRAD,      // (bindless_array, index: uint, uv: float3, ddx: float3, ddy: float3): float4
    BINDLESS_TEXTURE3D_SAMPLE_GRAD_LEVEL,// (bindless_array, index: uint, uv: float3, ddx: float3, ddy: float3, mip_clamp: float): float4

    BINDLESS_TEXTURE2D_SAMPLE_SAMPLER,           // (bindless_array, index: uint, uv: float2, filter: uint, level: uint): float4
    BINDLESS_TEXTURE2D_SAMPLE_LEVEL_SAMPLER,     // (bindless_array, index: uint, uv: float2, level: float, filter: uint, level: uint): float4
    BINDLESS_TEXTURE2D_SAMPLE_GRAD_SAMPLER,      // (bindless_array, index: uint, uv: float2, ddx: float2, ddy: float2, filter: uint, level: uint): float4
    BINDLESS_TEXTURE2D_SAMPLE_GRAD_LEVEL_SAMPLER,// (bindless_array, index: uint, uv: float2, ddx: float2, ddy: float2,  mip_clamp: float, filter: uint, level: uint): float4
    BINDLESS_TEXTURE3D_SAMPLE_SAMPLER,           // (bindless_array, index: uint, uv: float3, filter: uint, level: uint): float4
    BINDLESS_TEXTURE3D_SAMPLE_LEVEL_SAMPLER,     // (bindless_array, index: uint, uv: float3, level: float, filter: uint, level: uint): float4
    BINDLESS_TEXTURE3D_SAMPLE_GRAD_SAMPLER,      // (bindless_array, index: uint, uv: float3, ddx: float3, ddy: float3, filter: uint, level: uint): float4
    BINDLESS_TEXTURE3D_SAMPLE_GRAD_LEVEL_SAMPLER,// (bindless_array, index: uint, uv: float3, ddx: float3, ddy: float3,  mip_clamp: float, filter: uint, level: uint): float4

    BINDLESS_TEXTURE2D_READ,      // (bindless_array, index: uint, coord: uint2): float4
    BINDLESS_TEXTURE3D_READ,      // (bindless_array, index: uint, coord: uint3): float4
    BINDLESS_TEXTURE2D_READ_LEVEL,// (bindless_array, index: uint, coord: uint2, level: uint): float4
    BINDLESS_TEXTURE3D_READ_LEVEL,// (bindless_array, index: uint, coord: uint3, level: uint): float4
    BINDLESS_TEXTURE2D_SIZE,      // (bindless_array, index: uint): uint2
    BINDLESS_TEXTURE3D_SIZE,      // (bindless_array, index: uint): uint3
    BINDLESS_TEXTURE2D_SIZE_LEVEL,// (bindless_array, index: uint, level: uint): uint2
    BINDLESS_TEXTURE3D_SIZE_LEVEL,// (bindless_array, index: uint, level: uint): uint3

    BINDLESS_BUFFER_READ, // (bindless_array, index: uint, elem_index: uint) -> T
    BINDLESS_BUFFER_WRITE,// (bindless_array, index: uint, elem_index: uint, value: T) -> void
    BINDLESS_BUFFER_SIZE, // (bindless_array, index: uint, stride: uint) -> size: uint64

    BINDLESS_BYTE_BUFFER_READ, // (bindless_array, index: uint, offset_bytes: uint64) -> T
    BINDLESS_BYTE_BUFFER_WRITE,// (bindless_array, index: uint, offset_bytes: uint64, value: T) -> void
    BINDLESS_BYTE_BUFFER_SIZE, // (bindless_array, index: uint) -> size: uint64

    // low-level pointer operations, for akari
    BUFFER_DEVICE_ADDRESS,         // (buffer) -> address: uint64
    BINDLESS_BUFFER_DEVICE_ADDRESS,// (bindless_array, index: uint) -> address: uint64
    DEVICE_ADDRESS_READ,           // (address: uint64) -> value: T
    DEVICE_ADDRESS_WRITE,          // (address: uint64, value: T) -> void

    // autodiff ops
    AUTODIFF_REQUIRES_GRADIENT,  // (expr) -> void
    AUTODIFF_GRADIENT,           // (expr) -> expr
    AUTODIFF_GRADIENT_MARKER,    // (ref, expr) -> void
    AUTODIFF_ACCUMULATE_GRADIENT,// (ref, expr) -> void
    AUTODIFF_BACKWARD,           // (expr) -> void
    AUTODIFF_DETACH,             // (expr) -> expr

    // ray tracing
    RAY_TRACING_INSTANCE_TRANSFORM,      // (Accel, uint)
    RAY_TRACING_INSTANCE_USER_ID,        // (Accel, uint)
    RAY_TRACING_INSTANCE_VISIBILITY_MASK,// (Accel, uint)

    RAY_TRACING_SET_INSTANCE_TRANSFORM,      // (Accel, uint, float4x4)
    RAY_TRACING_SET_INSTANCE_VISIBILITY_MASK,// (Accel, uint, uint)
    RAY_TRACING_SET_INSTANCE_OPACITY,        // (Accel, uint, bool)
    RAY_TRACING_SET_INSTANCE_USER_ID,        // (Accel, uint, uint)

    RAY_TRACING_TRACE_CLOSEST,// (Accel, ray, mask: uint): TriangleHit
    RAY_TRACING_TRACE_ANY,    // (Accel, ray, mask: uint): bool
    RAY_TRACING_QUERY_ALL,    // (Accel, ray, mask: uint): RayQuery
    RAY_TRACING_QUERY_ANY,    // (Accel, ray, mask: uint): RayQuery

    // ray tracing with motion blur
    RAY_TRACING_INSTANCE_MOTION_MATRIX,    // (Accel, index: uint, key: uint): float4x4
    RAY_TRACING_INSTANCE_MOTION_SRT,       // (Accel, index: uint, key: uint): SRT
    RAY_TRACING_SET_INSTANCE_MOTION_MATRIX,// (Accel, index: uint, key: uint, transform: float4x4)
    RAY_TRACING_SET_INSTANCE_MOTION_SRT,   // (Accel, index: uint, key: uint, transform: SRT)

    RAY_TRACING_TRACE_CLOSEST_MOTION_BLUR,// (Accel, ray, time: float, mask: uint): TriangleHit
    RAY_TRACING_TRACE_ANY_MOTION_BLUR,    // (Accel, ray, time: float, mask: uint): bool
    RAY_TRACING_QUERY_ALL_MOTION_BLUR,    // (Accel, ray, time: float, mask: uint): RayQuery
    RAY_TRACING_QUERY_ANY_MOTION_BLUR,    // (Accel, ray, time: float, mask: uint): RayQuery

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

    // indirect dispatch
    INDIRECT_DISPATCH_SET_KERNEL,// (Buffer, uint offset, uint3 block_size, uint3 dispatch_size, uint kernel_id)
    INDIRECT_DISPATCH_SET_COUNT, // (Buffer, uint count)
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
