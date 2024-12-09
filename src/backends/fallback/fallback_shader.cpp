//
// Created by swfly on 2024/11/21.
//

#include <luisa/xir/translators/ast2xir.h>
#include <luisa/xir/translators/xir2text.h>
#include <luisa/core/stl.h>
#include <luisa/core/logging.h>
#include <luisa/core/clock.h>

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Passes/PassBuilder.h>

#include "fallback_codegen.h"
#include "fallback_texture.h"
#include "fallback_accel.h"
#include "fallback_bindless_array.h"
#include "fallback_shader.h"
#include "fallback_buffer.h"
#include "thread_pool.h"

namespace luisa::compute::fallback {

[[nodiscard]] static luisa::half luisa_asin_f16(luisa::half x) noexcept { return ::half_float::asin(x); }
[[nodiscard]] static float luisa_asin_f32(float x) noexcept { return std::asin(x); }
[[nodiscard]] static double luisa_asin_f64(double x) noexcept { return std::asin(x); }

[[nodiscard]] static luisa::half luisa_acos_f16(luisa::half x) noexcept { return ::half_float::acos(x); }
[[nodiscard]] static float luisa_acos_f32(float x) noexcept { return std::acos(x); }
[[nodiscard]] static double luisa_acos_f64(double x) noexcept { return std::acos(x); }

[[nodiscard]] static luisa::half luisa_atan_f16(luisa::half x) noexcept { return ::half_float::atan(x); }
[[nodiscard]] static float luisa_atan_f32(float x) noexcept { return std::atan(x); }
[[nodiscard]] static double luisa_atan_f64(double x) noexcept { return std::atan(x); }

[[nodiscard]] static luisa::half luisa_atan2_f16(luisa::half a, luisa::half b) noexcept { return ::half_float::atan2(a, b); }
[[nodiscard]] static float luisa_atan2_f32(float a, float b) noexcept { return std::atan2(a, b); }
[[nodiscard]] static double luisa_atan2_f64(double a, double b) noexcept { return std::atan2(a, b); }

struct FallbackShaderLaunchConfig {
    uint3 block_id;
    uint3 dispatch_size;
    uint3 block_size;
};

FallbackShader::FallbackShader(const ShaderOption &option, Function kernel) noexcept {

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

    map_symbol("texture.read.3d.float", &texture_read_3d_float_wrapper);
    map_symbol("texture.read.3d.uint", &texture_read_3d_uint_wrapper);

    map_symbol("accel.intersect.closest", &intersect_closest_wrapper);
    map_symbol("accel.intersect.any", &intersect_any_wrapper);
    map_symbol("accel.instance.transform", &accel_transform_wrapper);

    map_symbol("bindless.buffer.read", &bindless_buffer_read);
    map_symbol("bindless.tex2d", &bindless_tex2d_wrapper);
    map_symbol("bindless.tex2d.level", &bindless_tex2d_level_wrapper);
    map_symbol("bindless.tex2d.size", &bindless_tex2d_size_wrapper);

    // asin, acos, atan, atan2
    map_symbol("luisa.asin.f16", &luisa_asin_f16);
    map_symbol("luisa.asin.f32", &luisa_asin_f32);
    map_symbol("luisa.asin.f64", &luisa_asin_f64);
    map_symbol("luisa.acos.f16", &luisa_acos_f16);
    map_symbol("luisa.acos.f32", &luisa_acos_f32);
    map_symbol("luisa.acos.f64", &luisa_acos_f64);
    map_symbol("luisa.atan.f16", &luisa_atan_f16);
    map_symbol("luisa.atan.f32", &luisa_atan_f32);
    map_symbol("luisa.atan.f64", &luisa_atan_f64);
    map_symbol("luisa.atan2.f16", &luisa_atan2_f16);
    map_symbol("luisa.atan2.f32", &luisa_atan2_f32);
    map_symbol("luisa.atan2.f64", &luisa_atan2_f64);

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
    //    LUISA_INFO("Kernel XIR:\n{}", xir::xir_to_text_translate(xir_module, true));

    auto llvm_ctx = std::make_unique<llvm::LLVMContext>();
    auto llvm_module = luisa_fallback_backend_codegen(*llvm_ctx, xir_module);
    if (!llvm_module) {
        LUISA_ERROR_WITH_LOCATION("Failed to generate LLVM IR.");
    }
    //llvm_module->print(llvm::errs(), nullptr, true, true);
    //llvm_module->print(llvm::outs(), nullptr, true, true);
    if (llvm::verifyModule(*llvm_module, &llvm::errs())) {
        LUISA_ERROR_WITH_LOCATION("LLVM module verification failed.");
    }
    // {
    //     llvm_module->print(llvm::errs(), nullptr, true, true);
    //     // std::error_code EC;
    //     // llvm::raw_fd_ostream file_stream("H:/abc.ll", EC, llvm::sys::fs::OF_None);
    //     // llvm_module->print(file_stream, nullptr, true, true);
    //     // file_stream.close();
    // }

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
    _target_machine->registerPassBuilderCallbacks(PB, true);
#endif
    Clock clk;
    clk.tic();
    auto MPM = PB.buildPerModuleDefaultPipeline(::llvm::OptimizationLevel::O3);
    MPM.run(*llvm_module, MAM);
    LUISA_INFO("Optimized LLVM module in {} ms.", clk.toc());
    if (::llvm::verifyModule(*llvm_module, &::llvm::errs())) {
        LUISA_ERROR_WITH_LOCATION("Failed to verify module.");
    }
    //	{
    //		std::error_code EC;
    //		llvm::raw_fd_ostream file_stream("bbc.ll", EC, llvm::sys::fs::OF_None);
    //		llvm_module->print(file_stream, nullptr, true, true);
    //		file_stream.close();
    //	}
    llvm_module->print(llvm::errs(), nullptr, false, true);

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

    // compute argument buffer size
    _argument_buffer_size = 0u;
    static constexpr auto argument_alignment = 16u;
    for (auto arg : kernel.arguments()) {
        switch (arg.tag()) {
            case Variable::Tag::LOCAL: {
                _argument_buffer_size += arg.type()->size();
                _argument_buffer_size = luisa::align(_argument_buffer_size, argument_alignment);
                break;
            }
            case Variable::Tag::BUFFER: {
                _argument_buffer_size += sizeof(FallbackBufferView);
                _argument_buffer_size = luisa::align(_argument_buffer_size, argument_alignment);
                break;
            }
            case Variable::Tag::TEXTURE: {
                _argument_buffer_size += sizeof(FallbackTextureView);
                _argument_buffer_size = luisa::align(_argument_buffer_size, argument_alignment);
                break;
            }
            case Variable::Tag::BINDLESS_ARRAY: {
                _argument_buffer_size += sizeof(FallbackBindlessArray *);
                _argument_buffer_size = luisa::align(_argument_buffer_size, argument_alignment);
                break;
            }
            case Variable::Tag::ACCEL: {
                _argument_buffer_size += sizeof(FallbackAccel *);
                _argument_buffer_size = luisa::align(_argument_buffer_size, argument_alignment);
                break;
            }
            default: LUISA_ERROR_WITH_LOCATION("Unsupported argument type.");
        }
    }
}

void FallbackShader::dispatch(ThreadPool &pool, const ShaderDispatchCommand *command) const noexcept {

    auto argument_buffer = std::make_shared<std::byte[]>(_argument_buffer_size);

    auto argument_buffer_offset = static_cast<size_t>(0u);
    auto allocate_argument = [&](size_t bytes) noexcept {
        static constexpr auto alignment = 16u;
        auto offset = (argument_buffer_offset + alignment - 1u) / alignment * alignment;
        LUISA_ASSERT(offset + bytes <= _argument_buffer_size,
                     "Too many arguments in ShaderDispatchCommand");
        argument_buffer_offset = offset + bytes;
        return argument_buffer.get() + offset;
    };

    auto encode_argument = [&allocate_argument, command](const auto &arg) noexcept {
        using Tag = ShaderDispatchCommand::Argument::Tag;
        switch (arg.tag) {
            case Tag::BUFFER: {
                auto buffer = reinterpret_cast<FallbackBuffer *>(arg.buffer.handle);
                auto buffer_view = buffer->view(arg.buffer.offset);
                auto ptr = allocate_argument(sizeof(buffer_view));
                std::memcpy(ptr, &buffer_view, sizeof(buffer_view));
                break;
            }
            case Tag::TEXTURE: {
                auto texture = reinterpret_cast<const FallbackTexture *>(arg.texture.handle);
                auto view = texture->view(arg.texture.level);
                auto ptr = allocate_argument(sizeof(view));
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

    auto round_up_division = [](unsigned a, unsigned b) {
        return (a + b - 1) / b;
    };
    auto dispatch_size = command->dispatch_size();
    auto dispatch_counts = make_uint3(
        round_up_division(dispatch_size.x, _block_size.x),
        round_up_division(dispatch_size.y, _block_size.y),
        round_up_division(dispatch_size.z, _block_size.z));

    //     for (int i = 0; i < dispatch_counts.x; ++i) {
    //         for (int j = 0; j < dispatch_counts.y; ++j) {
    //             for (int k = 0; k < dispatch_counts.z; ++k) {
    //                 auto c = config;
    //                 c.block_id = make_uint3(i, j, k);
    //                 (*_kernel_entry)(data, &c);
    //             }
    //         }
    //     }

    pool.parallel(dispatch_counts.x, dispatch_counts.y, dispatch_counts.z,
                  [this, argument_buffer = std::move(argument_buffer), dispatch_size](auto bx, auto by, auto bz) noexcept {
                      FallbackShaderLaunchConfig config{
                          .block_id = make_uint3(bx, by, bz),
                          .dispatch_size = dispatch_size,
                          .block_size = _block_size,
                      };
                      (*_kernel_entry)(argument_buffer.get(), &config);
                  });
}

void FallbackShader::build_bound_arguments(compute::Function kernel) {
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

}// namespace luisa::compute::fallback
