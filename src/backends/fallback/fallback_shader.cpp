//
// Created by swfly on 2024/11/21.
//

#include "fallback_shader.h"

#include "fallback_buffer.h"

#include <luisa/xir/translators/ast2xir.h>
#include <luisa/xir/translators/xir2text.h>
#include <luisa/core/stl.h>
#include <luisa/core/logging.h>
#include <luisa/core/clock.h>

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/Verifier.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Passes/PassBuilder.h>

#include "fallback_codegen.h"
#include "fallback_texture.h"
#include "fallback_accel.h"
#include "fallback_bindless_array.h"
#include "thread_pool.h"

using namespace luisa;
luisa::compute::fallback::FallbackShader::FallbackShader(const luisa::compute::ShaderOption &option, luisa::compute::Function kernel) noexcept {

    // build JIT engine
    ::llvm::orc::LLJITBuilder jit_builder;
    if (auto host = ::llvm::orc::JITTargetMachineBuilder::detectHost()) {
        ::llvm::TargetOptions options;
        options.AllowFPOpFusion = ::llvm::FPOpFusion::Fast;
        options.UnsafeFPMath = true;
        options.NoInfsFPMath = true;
        options.NoNaNsFPMath = true;
        options.NoTrappingFPMath = true;
        options.NoSignedZerosFPMath = true;
        options.ApproxFuncFPMath = true;
        options.EnableIPRA = true;
        options.StackSymbolOrdering = true;
        options.EnableMachineFunctionSplitter = true;
        options.EnableMachineOutliner = true;
        options.NoTrapAfterNoreturn = true;
        host->setOptions(options);
        host->setCodeGenOptLevel(::llvm::CodeGenOptLevel::Aggressive);
#ifdef __aarch64__
        host->addFeatures({"+neon"});
#else
        host->addFeatures({"+avx2"});
#endif
        // LUISA_INFO("LLVM JIT target: triplet = {}, features = {}.",
        //            host->getTargetTriple().str(),
        //            host->getFeatures().getString());
        if (auto machine = host->createTargetMachine()) {
            _target_machine = std::move(machine.get());
        } else {
            ::llvm::handleAllErrors(machine.takeError(), [&](const ::llvm::ErrorInfoBase &e) {
                LUISA_WARNING_WITH_LOCATION("JITTargetMachineBuilder::createTargetMachine(): {}.", e.message());
            });
            LUISA_ERROR_WITH_LOCATION("Failed to create target machine.");
        }
        jit_builder.setJITTargetMachineBuilder(std::move(*host));
    } else {
        ::llvm::handleAllErrors(host.takeError(), [&](const ::llvm::ErrorInfoBase &e) {
            LUISA_WARNING_WITH_LOCATION("JITTargetMachineBuilder::detectHost(): {}.", e.message());
        });
        LUISA_ERROR_WITH_LOCATION("Failed to detect host.");
    }

    if (auto expected_jit = jit_builder.create()) {
        _jit = std::move(expected_jit.get());
    } else {
        ::llvm::handleAllErrors(expected_jit.takeError(), [](const ::llvm::ErrorInfoBase &err) {
            LUISA_WARNING_WITH_LOCATION("LLJITBuilder::create(): {}", err.message());
        });
        LUISA_ERROR_WITH_LOCATION("Failed to create LLJIT.");
    }

    // map symbols
    llvm::orc::SymbolMap symbol_map{};
    auto map_symbol = [jit = _jit.get(), &symbol_map]<typename T>(const char *name, T *f) noexcept {
        static_assert(std::is_function_v<T>);
        auto addr = llvm::orc::ExecutorAddr::fromPtr(f);
        auto symbol = llvm::orc::ExecutorSymbolDef{addr, llvm::JITSymbolFlags::Exported};
        symbol_map.try_emplace(jit->mangleAndIntern(name), symbol);
    };
    map_symbol("texture.write.2d.float", &texture_write_2d_float_wrapper);
    map_symbol("texture.read.2d.float", &texture_read_2d_float_wrapper);
    map_symbol("texture.write.2d.uint", &texture_write_2d_uint_wrapper);
    map_symbol("texture.read.2d.uint", &texture_read_2d_uint_wrapper);

    map_symbol("intersect.closest", &intersect_closest_wrapper);

    map_symbol("bindless.buffer.read", &bindless_buffer_read);
    if (auto error = _jit->getMainJITDylib().define(
            ::llvm::orc::absoluteSymbols(std::move(symbol_map)))) {
        ::llvm::handleAllErrors(std::move(error), [](const ::llvm::ErrorInfoBase &err) {
            LUISA_WARNING_WITH_LOCATION("LLJIT::define(): {}", err.message());
        });
        LUISA_ERROR_WITH_LOCATION("Failed to define symbols.");
    }

    if (auto generator = ::llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
            _jit->getDataLayout().getGlobalPrefix())) {
        _jit->getMainJITDylib().addGenerator(std::move(generator.get()));
    } else {
        ::llvm::handleAllErrors(generator.takeError(), [](const ::llvm::ErrorInfoBase &err) {
            LUISA_WARNING_WITH_LOCATION("DynamicLibrarySearchGenerator::GetForCurrentProcess(): {}", err.message());
        });
        LUISA_ERROR_WITH_LOCATION("Failed to add generator.");
    }

    _block_size = kernel.block_size();
    build_bound_arguments(kernel);
    xir::Pool pool;
    xir::PoolGuard guard{&pool};
    auto xir_module = xir::ast_to_xir_translate(kernel, {});
    xir_module->set_name(luisa::format("kernel_{:016x}", kernel.hash()));
    if (!option.name.empty()) { xir_module->set_location(option.name); }
    LUISA_INFO("Kernel XIR:\n{}", xir::xir_to_text_translate(xir_module, true));

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

    // optimize
    llvm_module->setDataLayout(_target_machine->createDataLayout());
    llvm_module->setTargetTriple(_target_machine->getTargetTriple().str());

    // optimize with the new pass manager
    ::llvm::LoopAnalysisManager LAM;
    ::llvm::FunctionAnalysisManager FAM;
    ::llvm::CGSCCAnalysisManager CGAM;
    ::llvm::ModuleAnalysisManager MAM;
    ::llvm::PipelineTuningOptions PTO;
    PTO.LoopInterleaving = true;
    PTO.LoopVectorization = true;
    PTO.SLPVectorization = true;
    PTO.LoopUnrolling = true;
    PTO.MergeFunctions = true;
    ::llvm::PassBuilder PB{_target_machine.get(), PTO};
    FAM.registerPass([&] { return PB.buildDefaultAAPipeline(); });
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
#if LLVM_VERSION_MAJOR >= 19
    _target_machine->registerPassBuilderCallbacks(PB);
#else
    _target_machine->registerPassBuilderCallbacks(PB, false);
#endif
    Clock clk;
    clk.tic();
    auto MPM = PB.buildPerModuleDefaultPipeline(::llvm::OptimizationLevel::O3);
    MPM.run(*llvm_module, MAM);
    LUISA_INFO("Optimized LLVM module in {} ms.", clk.toc());
    if (::llvm::verifyModule(*llvm_module, &::llvm::errs())) {
        LUISA_ERROR_WITH_LOCATION("Failed to verify module.");
    }
    llvm_module->print(llvm::outs(), nullptr, true, true);

    // compile to machine code
    auto m = llvm::orc::ThreadSafeModule(std::move(llvm_module), std::move(llvm_ctx));
    if (auto error = _jit->addIRModule(std::move(m))) {
        ::llvm::handleAllErrors(std::move(error), [](const ::llvm::ErrorInfoBase &err) {
            LUISA_WARNING_WITH_LOCATION("LLJIT::addIRModule(): {}", err.message());
        });
    }
    auto addr = _jit->lookup("kernel.main");
    if (!addr) {
        ::llvm::handleAllErrors(addr.takeError(), [](const ::llvm::ErrorInfoBase &err) {
            LUISA_WARNING_WITH_LOCATION("LLJIT::lookup(): {}", err.message());
        });
    }
    LUISA_ASSERT(addr, "JIT compilation failed with error [{}]");
    _kernel_entry = addr->toPtr<kernel_entry_t>();
}

void compute::fallback::FallbackShader::dispatch(ThreadPool &pool, const compute::ShaderDispatchCommand *command) const noexcept {
    thread_local std::array<std::byte, 65536u> argument_buffer;// should be enough

    auto argument_buffer_offset = static_cast<size_t>(0u);
    auto allocate_argument = [&](size_t bytes) noexcept {
        static constexpr auto alignment = 16u;
        auto offset = (argument_buffer_offset + alignment - 1u) / alignment * alignment;
        LUISA_ASSERT(offset + bytes <= argument_buffer.size(),
                     "Too many arguments in ShaderDispatchCommand");
        argument_buffer_offset = offset + bytes;
        return argument_buffer.data() + offset;
    };

    auto encode_argument = [&allocate_argument, command](const auto &arg) noexcept {
        using Tag = ShaderDispatchCommand::Argument::Tag;
        switch (arg.tag) {
            case Tag::BUFFER: {
                //What is indirect?
                //                if (reinterpret_cast<const CUDABufferBase *>(arg.buffer.handle)->is_indirect())
                //                {
                //                    auto buffer = reinterpret_cast<const CUDAIndirectDispatchBuffer *>(arg.buffer.handle);
                //                    auto binding = buffer->binding(arg.buffer.offset, arg.buffer.size);
                //                    auto ptr = allocate_argument(sizeof(binding));
                //                    std::memcpy(ptr, &binding, sizeof(binding));
                //                }
                //                else
                {
                    auto buffer = reinterpret_cast<FallbackBuffer *>(arg.buffer.handle);
                    auto buffer_view = buffer->view(arg.buffer.offset);
                    //auto binding = buffer->binding(arg.buffer.offset, arg.buffer.size);
                    auto ptr = allocate_argument(sizeof(buffer_view));
                    std::memcpy(ptr, &buffer, sizeof(buffer_view));
                }
                break;
            }
            case Tag::TEXTURE: {
                auto texture = reinterpret_cast<const FallbackTexture *>(arg.texture.handle);
                auto view = texture->view(arg.texture.level);
                auto ptr = allocate_argument(sizeof(view));
                FallbackTextureView *v = reinterpret_cast<FallbackTextureView *>(ptr);
                std::memcpy(ptr, &view, sizeof(view));
                break;
            }
            case Tag::UNIFORM: {
                auto uniform = command->uniform(arg.uniform);
                auto ptr = allocate_argument(uniform.size_bytes());
                std::memcpy(ptr, uniform.data(), uniform.size_bytes());
                break;
            }
            case Tag::BINDLESS_ARRAY: {
                auto bindless = reinterpret_cast<FallbackBindlessArray *>(arg.buffer.handle);
                //auto binding = buffer->binding(arg.buffer.offset, arg.buffer.size);
                auto ptr = allocate_argument(sizeof(bindless));
                std::memcpy(ptr, &bindless, sizeof(bindless));
                break;
            }
            case Tag::ACCEL: {
                auto accel = reinterpret_cast<FallbackAccel *>(arg.accel.handle);
                auto ptr = allocate_argument(sizeof(accel));
                std::memcpy(ptr, &accel, sizeof(accel));
                break;
            }
        }
    };
    for (auto &&arg : _bound_arguments) { encode_argument(arg); }
    for (auto &&arg : command->arguments()) { encode_argument(arg); }
    struct LaunchConfig {
        uint3 block_id;
        uint3 dispatch_size;
        uint3 block_size;
    };
    // TODO: fill in true values
    LaunchConfig config{
        .block_id = make_uint3(0u),
        .dispatch_size = command->dispatch_size(),
        .block_size = _block_size,
    };

    auto round_up_division = [](unsigned a, unsigned b) {
        return (a + b - 1) / b;
    };
    auto dispatch_counts = make_uint3(
        round_up_division(config.dispatch_size.x, _block_size.x),
        round_up_division(config.dispatch_size.y, _block_size.y),
        round_up_division(config.dispatch_size.z, _block_size.z));

    auto data = argument_buffer.data();

    // for (int i = 0; i < dispatch_counts.x; ++i) {
    //     for (int j = 0; j < dispatch_counts.y; ++j) {
    //         for (int k = 0; k < dispatch_counts.z; ++k) {
    //             auto c = config;
    //             c.block_id = make_uint3(i, j, k);
    //             (*_kernel_entry)(data, &c);
    //         }
    //     }
    // }

    pool.parallel(dispatch_counts.x, dispatch_counts.y, dispatch_counts.z,
       [this, config, data](auto bx, auto by, auto bz) noexcept {
            auto c = config;
           c.block_id = make_uint3(bx, by, bz);
           (*_kernel_entry)(data, &c);
    });
    pool.barrier();
}
void compute::fallback::FallbackShader::build_bound_arguments(compute::Function kernel) {
    _bound_arguments.reserve(kernel.bound_arguments().size());
    for (auto &&arg : kernel.bound_arguments()) {
        luisa::visit(
            [&]<typename T>(T binding) noexcept {
                ShaderDispatchCommand::Argument argument{};
                if constexpr (std::is_same_v<T, Function::BufferBinding>) {
                    argument.tag = ShaderDispatchCommand::Argument::Tag::BUFFER;
                    argument.buffer.handle = binding.handle;
                    argument.buffer.offset = binding.offset;
                    argument.buffer.size = binding.size;
                } else if constexpr (std::is_same_v<T, Function::TextureBinding>) {
                    argument.tag = ShaderDispatchCommand::Argument::Tag::TEXTURE;
                    argument.texture.handle = binding.handle;
                    argument.texture.level = binding.level;
                } else if constexpr (std::is_same_v<T, Function::BindlessArrayBinding>) {
                    argument.tag = ShaderDispatchCommand::Argument::Tag::BINDLESS_ARRAY;
                    argument.bindless_array.handle = binding.handle;
                } else if constexpr (std::is_same_v<T, Function::AccelBinding>) {
                    argument.tag = ShaderDispatchCommand::Argument::Tag::ACCEL;
                    argument.accel.handle = binding.handle;
                } else {
                    LUISA_ERROR_WITH_LOCATION("Unsupported binding type.");
                }
                _bound_arguments.emplace_back(argument);
            },
            arg);
    }
}
