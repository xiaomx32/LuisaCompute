#pragma once

#include <luisa/core/dll_export.h>
#include <luisa/core/stl/unordered_map.h>

namespace luisa::compute::xir {

class Value;
class LoadInst;
class StoreInst;
class BasicBlock;
class Function;
class Module;

// This pass is used to forward stores to loads for scalar variables
// within straight-line basic blocks. It is a simple peephole optimization
// that can be used to reduce the number of memory operations.

struct LocalStoreForwardInfo {
    luisa::unordered_map<LoadInst *, StoreInst *> forwarded_instructions;
};

[[nodiscard]] LC_XIR_API LocalStoreForwardInfo local_store_forward_pass_run_on_function(Function *function) noexcept;
[[nodiscard]] LC_XIR_API LocalStoreForwardInfo local_store_forward_pass_run_on_module(Module *module) noexcept;

}// namespace luisa::compute::xir
