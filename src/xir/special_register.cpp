#include <luisa/core/logging.h>
#include <luisa/ast/type_registry.h>
#include <luisa/xir/special_register.h>

namespace luisa::compute::xir {

namespace detail {
const Type *special_register_type_uint() noexcept { return Type::of<uint>(); }
const Type *special_register_type_uint3() noexcept { return Type::of<uint3>(); }
}// namespace detail

SpecialRegister *SpecialRegister::create(DerivedSpecialRegisterTag tag) noexcept {
    switch (tag) {
        case DerivedSpecialRegisterTag::THREAD_ID: return Pool::current()->create<SPR_ThreadID>();
        case DerivedSpecialRegisterTag::BLOCK_ID: return Pool::current()->create<SPR_BlockID>();
        case DerivedSpecialRegisterTag::WARP_LANE_ID: return Pool::current()->create<SPR_WarpLaneID>();
        case DerivedSpecialRegisterTag::DISPATCH_ID: return Pool::current()->create<SPR_DispatchID>();
        case DerivedSpecialRegisterTag::KERNEL_ID: return Pool::current()->create<SPR_KernelID>();
        case DerivedSpecialRegisterTag::OBJECT_ID: return Pool::current()->create<SPR_ObjectID>();
        case DerivedSpecialRegisterTag::BLOCK_SIZE: return Pool::current()->create<SPR_BlockSize>();
        case DerivedSpecialRegisterTag::WARP_SIZE: return Pool::current()->create<SPR_WarpSize>();
        case DerivedSpecialRegisterTag::DISPATCH_SIZE: return Pool::current()->create<SPR_DispatchSize>();
    }
    LUISA_ERROR_WITH_LOCATION("Unexpected special register tag.");
}

}// namespace luisa::compute::xir
