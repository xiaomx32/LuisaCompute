#pragma once

#include <luisa/xir/value.h>

namespace luisa::compute::xir {

enum struct DerivedSpecialRegisterTag {
    THREAD_ID,
    BLOCK_ID,
    WARP_LANE_ID,
    DISPATCH_ID,
    KERNEL_ID,
    OBJECT_ID,
    BLOCK_SIZE,
    WARP_SIZE,
    DISPATCH_SIZE,
};

[[nodiscard]] constexpr luisa::string_view to_string(DerivedSpecialRegisterTag tag) noexcept {
    using namespace std::string_view_literals;
    switch (tag) {
        case DerivedSpecialRegisterTag::THREAD_ID: return "thread_id"sv;
        case DerivedSpecialRegisterTag::BLOCK_ID: return "block_id"sv;
        case DerivedSpecialRegisterTag::WARP_LANE_ID: return "warp_lane_id"sv;
        case DerivedSpecialRegisterTag::DISPATCH_ID: return "dispatch_id"sv;
        case DerivedSpecialRegisterTag::KERNEL_ID: return "kernel_id"sv;
        case DerivedSpecialRegisterTag::OBJECT_ID: return "object_id"sv;
        case DerivedSpecialRegisterTag::BLOCK_SIZE: return "block_size"sv;
        case DerivedSpecialRegisterTag::WARP_SIZE: return "warp_size"sv;
        case DerivedSpecialRegisterTag::DISPATCH_SIZE: return "dispatch_size"sv;
    }
    return "unknown"sv;
}

class LC_XIR_API SpecialRegister : public DerivedValue<DerivedValueTag::SPECIAL_REGISTER> {
public:
    explicit SpecialRegister(const Type *type) noexcept : DerivedValue{type} {}
    [[nodiscard]] virtual DerivedSpecialRegisterTag derived_special_register_tag() const noexcept = 0;
    [[nodiscard]] static SpecialRegister *create(DerivedSpecialRegisterTag tag) noexcept;
};

namespace detail {

[[nodiscard]] LC_XIR_API const Type *special_register_type_uint() noexcept;
[[nodiscard]] LC_XIR_API const Type *special_register_type_uint3() noexcept;

template<typename T>
[[nodiscard]] auto get_special_register_type() noexcept {
    if constexpr (std::is_same_v<T, uint>) {
        return special_register_type_uint();
    } else if constexpr (std::is_same_v<T, uint3>) {
        return special_register_type_uint3();
    } else {
        static_assert(always_false_v<T>, "Unsupported special register type.");
    }
}

}// namespace detail

template<typename T, DerivedSpecialRegisterTag tag>
class DerivedSpecialRegister : public SpecialRegister {
public:
    DerivedSpecialRegister() noexcept : SpecialRegister{detail::get_special_register_type<T>()} {}
    [[nodiscard]] static constexpr auto
    static_derived_special_register_tag() noexcept { return tag; }
    [[nodiscard]] DerivedSpecialRegisterTag
    derived_special_register_tag() const noexcept final {
        return static_derived_special_register_tag();
    }
    [[nodiscard]] static auto create() noexcept {
        return static_cast<DerivedSpecialRegister *>(SpecialRegister::create(tag));
    }
};

// special registers
// note that we add the `SPR` prefix to avoid potential name conflicts with macros
using SPR_ThreadID = DerivedSpecialRegister<uint3, DerivedSpecialRegisterTag::THREAD_ID>;
using SPR_BlockID = DerivedSpecialRegister<uint3, DerivedSpecialRegisterTag::BLOCK_ID>;
using SPR_WarpLaneID = DerivedSpecialRegister<uint, DerivedSpecialRegisterTag::WARP_LANE_ID>;
using SPR_DispatchID = DerivedSpecialRegister<uint3, DerivedSpecialRegisterTag::DISPATCH_ID>;
using SPR_KernelID = DerivedSpecialRegister<uint, DerivedSpecialRegisterTag::KERNEL_ID>;
using SPR_ObjectID = DerivedSpecialRegister<uint, DerivedSpecialRegisterTag::OBJECT_ID>;
using SPR_BlockSize = DerivedSpecialRegister<uint3, DerivedSpecialRegisterTag::BLOCK_SIZE>;
using SPR_WarpSize = DerivedSpecialRegister<uint, DerivedSpecialRegisterTag::WARP_SIZE>;
using SPR_DispatchSize = DerivedSpecialRegister<uint3, DerivedSpecialRegisterTag::DISPATCH_SIZE>;

}// namespace luisa::compute::xir
