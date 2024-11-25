//
// Created by swfly on 2024/11/21.
//

#pragma once

#include <luisa/core/stl/unordered_map.h>
#include <luisa/ast/function.h>
#include <luisa/ast/function_builder.h>
#include <luisa/runtime/rhi/resource.h>
#include <luisa/runtime/rhi/command.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>

namespace llvm {
class LLVMContext;
class Module;
class ExecutionEngine;
}// namespace llvm

namespace llvm::orc {
class LLJIT;
}// namespace llvm::orc
namespace luisa
{
    class ThreadPool;
}

namespace luisa::compute::fallback {

using luisa::compute::detail::FunctionBuilder;

class FallbackShader {

public:
    using kernel_entry_t = void(const void *, const void *);

private:
    luisa::string _name;
    luisa::unordered_map<uint, size_t> _argument_offsets;
    kernel_entry_t *_kernel_entry{nullptr};
    size_t _argument_buffer_size{};
    //luisa::vector<CpuCallback> _callbacks;
    size_t _shared_memory_size{};
    unique_ptr<llvm::Module> _module{};
    luisa::vector<ShaderDispatchCommand::Argument> _bound_arguments;

    uint3 _block_size;
    mutable std::unique_ptr<::llvm::orc::LLJIT> _jit;
    std::unique_ptr<::llvm::TargetMachine> _target_machine;


    void build_bound_arguments(Function kernel);
public:

    void dispatch(ThreadPool& pool, const ShaderDispatchCommand *command) const noexcept;
    FallbackShader(const ShaderOption &option, Function kernel) noexcept;
    ~FallbackShader() noexcept;

    [[nodiscard]] auto argument_buffer_size() const noexcept { return _argument_buffer_size; }
    [[nodiscard]] auto shared_memory_size() const noexcept { return _shared_memory_size; }
    [[nodiscard]] size_t argument_offset(uint uid) const noexcept;
    //[[nodiscard]] auto callbacks() const noexcept { return _callbacks.data(); }
};

}// namespace luisa::compute::fallback
