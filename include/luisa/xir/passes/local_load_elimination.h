#pragma once

#include <luisa/core/dll_export.h>
#include <luisa/core/stl/unordered_map.h>

namespace luisa::compute::xir {

class LoadInst;
class Function;
class Module;

struct LocalLoadEliminationInfo {
    luisa::unordered_map<LoadInst *, LoadInst *> eliminated_instructions;
};

[[nodiscard]] LC_XIR_API LocalLoadEliminationInfo local_load_elimination_pass_run_on_function(Function *function) noexcept;
[[nodiscard]] LC_XIR_API LocalLoadEliminationInfo local_load_elimination_pass_run_on_module(Module *module) noexcept;

}// namespace luisa::compute::xir
