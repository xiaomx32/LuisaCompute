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
    build_bound_arguments(kernel);
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
    auto addr = jit->lookup("kernel.main");
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

void compute::fallback::FallbackShader::dispatch(const compute::ShaderDispatchCommand *command)const noexcept
{
    static thread_local std::array<std::byte, 65536u> argument_buffer;// should be enough

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
                    auto buffer = reinterpret_cast<const void*>(arg.buffer.handle);
                    //auto binding = buffer->binding(arg.buffer.offset, arg.buffer.size);
                    auto ptr = allocate_argument(sizeof(buffer));
                    std::memcpy(ptr, &buffer, sizeof(buffer));
                }
                break;
            }
            case Tag::TEXTURE: {
//                auto texture = reinterpret_cast<const CUDATexture *>(arg.texture.handle);
//                auto binding = texture->binding(arg.texture.level);
//                auto ptr = allocate_argument(sizeof(binding));
//                std::memcpy(ptr, &binding, sizeof(binding));
                break;
            }
            case Tag::UNIFORM: {
                auto uniform = command->uniform(arg.uniform);
                auto ptr = allocate_argument(uniform.size_bytes());
                std::memcpy(ptr, uniform.data(), uniform.size_bytes());
                break;
            }
            case Tag::BINDLESS_ARRAY: {
//                auto array = reinterpret_cast<const CUDABindlessArray *>(arg.bindless_array.handle);
//                auto binding = array->binding();
//                auto ptr = allocate_argument(sizeof(binding));
//                std::memcpy(ptr, &binding, sizeof(binding));
                break;
            }
            case Tag::ACCEL: {
//                auto accel = reinterpret_cast<const CUDAAccel *>(arg.accel.handle);
//                auto binding = accel->binding();
//                auto ptr = allocate_argument(sizeof(binding));
//                std::memcpy(ptr, &binding, sizeof(binding));
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
        .block_size = make_uint3(0u),
    };
    (*_kernel_entry)(argument_buffer.data(), argument_buffer.data());
}
void compute::fallback::FallbackShader::build_bound_arguments(compute::Function kernel)
{;
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
