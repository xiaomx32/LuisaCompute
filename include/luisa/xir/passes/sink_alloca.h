#pragma once

#include <luisa/core/stl/vector.h>
#include <luisa/xir/module.h>

namespace luisa::compute::xir {

class AllocaInst;

struct SinkAllocaInfo {
    luisa::vector<AllocaInst *> sunken_instructions;
};

LC_XIR_API SinkAllocaInfo sink_alloca_pass_run_on_function(Function *function) noexcept;
LC_XIR_API SinkAllocaInfo sink_alloca_pass_run_on_module(Module *module) noexcept;

}// namespace luisa::compute::xir
