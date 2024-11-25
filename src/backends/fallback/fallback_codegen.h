#pragma once

namespace llvm {
class Module;
class LLVMContext;
}// namespace llvm

namespace luisa::compute::xir {
class Module;
}// namespace luisa::compute::xir

namespace luisa::compute::fallback {

[[nodiscard]] std::unique_ptr<llvm::Module>
luisa_fallback_backend_codegen(llvm::LLVMContext &llvm_ctx,
                               const xir::Module *module) noexcept;

}// namespace luisa::compute::fallback
