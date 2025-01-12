#pragma once

#include <luisa/core/dll_export.h>

namespace luisa::compute::xir {

class AllocaInst;
class Value;

[[nodiscard]] LC_XIR_API AllocaInst *trace_pointer_base_local_alloca_inst(Value *pointer) noexcept;

}// namespace luisa::compute::xir
