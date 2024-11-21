//
// Created by swfly on 2024/11/21.
//

#include "fallback_shader.h"
#include <luisa/xir/translators/ast2xir.h>
#include <luisa/xir/translators/xir2text.h>
#include <luisa/core/stl.h>
#include <luisa/core/logging.h>


#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/Verifier.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

#include "fallback_codegen.h"

using namespace luisa;
luisa::compute::fallback::FallbackShader::FallbackShader(llvm::orc::LLJIT *jit, const luisa::compute::ShaderOption &option, luisa::compute::Function kernel) noexcept
{
    xir::Pool pool;
    xir::PoolGuard guard{&pool};
    auto xir_module = xir::ast_to_xir_translate(kernel, {});
    xir_module->set_name(luisa::format("kernel_{:016x}", kernel.hash()));
    if (!option.name.empty()) { xir_module->set_location(option.name); }
    // LUISA_INFO("Kernel XIR:\n{}", xir::xir_to_text_translate(xir_module, true));

    auto llvm_ctx = std::make_unique<llvm::LLVMContext>();
    auto llvm_module = luisa_fallback_backend_codegen(*llvm_ctx, xir_module);
    if (!llvm_module) {
        LUISA_ERROR_WITH_LOCATION("Failed to generate LLVM IR.");
    }
    //llvm_module->print(llvm::errs(), nullptr, true, true);
    llvm_module->print(llvm::outs(), nullptr, true, true);
    if (llvm::verifyModule(*llvm_module, &llvm::errs())) {
        LUISA_ERROR_WITH_LOCATION("LLVM module verification failed.");
    }


    // compile to machine code
    auto m = llvm::orc::ThreadSafeModule(std::move(llvm_module), std::move(llvm_ctx));
    auto error = jit->addIRModule(std::move(m));
    if(error)
    {
        ::llvm::handleAllErrors(std::move(error), [](const ::llvm::ErrorInfoBase &err) {
            LUISA_WARNING_WITH_LOCATION("LLJIT::addIRModule(): {}", err.message());
        });
    }
    auto addr = jit->lookup("kernel");
    if(!addr)
    {
        ::llvm::handleAllErrors(addr.takeError(), [](const ::llvm::ErrorInfoBase &err)
        {
            LUISA_WARNING_WITH_LOCATION("LLJIT::lookup(): {}", err.message());
        });
    }
    LUISA_ASSERT(addr, "JIT compilation failed with error [{}]");
    _kernel_entry = addr->toPtr<kernel_entry_t>();

}
