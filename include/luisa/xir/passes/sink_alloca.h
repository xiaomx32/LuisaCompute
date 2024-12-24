#pragma once

#include <luisa/core/stl/vector.h>
#include <luisa/xir/module.h>

namespace luisa::compute::xir {

class AllocaInst;

struct SinkAllocaInfo {
    luisa::vector<AllocaInst *> sunken_alloca_insts;
};

LC_XIR_API SinkAllocaInfo sink_alloca_pass_run_on_function(Module *module, Function *function) noexcept;
LC_XIR_API SinkAllocaInfo sink_alloca_pass_run_on_module(Module *module) noexcept;

}// namespace luisa::compute::xir
