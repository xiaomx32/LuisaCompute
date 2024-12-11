#pragma once

namespace llvm {
class Module;
class LLVMContext;
}// namespace llvm

namespace luisa::compute::xir {
class Module;
}// namespace luisa::compute::xir

namespace luisa::compute::fallback {

void luisa_fallback_backend_codegen(llvm::LLVMContext &llvm_ctx,
                                    llvm::Module *llvm_module,
                                    const xir::Module *module) noexcept;

}// namespace luisa::compute::fallback
