//
// Created by swfly on 2024/11/21.
//

#pragma once


#include <luisa/core/stl/unordered_map.h>
#include <luisa/ast/function.h>
#include <luisa/ast/function_builder.h>
#include <luisa/runtime/rhi/resource.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>

namespace llvm {
class LLVMContext;
class Module;
class ExecutionEngine;
}// namespace llvm

namespace llvm::orc {
class LLJIT;
}// namespace llvm::orc

namespace luisa::compute::fallback {

using luisa::compute::detail::FunctionBuilder;

class FallbackShader {

public:
    using kernel_entry_t = void(void*, void*);

private:
    luisa::string _name;
    luisa::unordered_map<uint, size_t> _argument_offsets;
    kernel_entry_t *_kernel_entry{nullptr};
    size_t _argument_buffer_size{};
    //luisa::vector<CpuCallback> _callbacks;
    size_t _shared_memory_size{};
    unique_ptr<llvm::Module> _module{};

public:
    FallbackShader(llvm::orc::LLJIT* jit, const ShaderOption &option, Function kernel) noexcept;
    ~FallbackShader() noexcept;
    [[nodiscard]] auto argument_buffer_size() const noexcept { return _argument_buffer_size; }
    [[nodiscard]] auto shared_memory_size() const noexcept { return _shared_memory_size; }
    [[nodiscard]] size_t argument_offset(uint uid) const noexcept;
    //[[nodiscard]] auto callbacks() const noexcept { return _callbacks.data(); }
    void invoke(const std::byte *args, std::byte *shared_memory,
                uint3 dispatch_size, uint3 block_id) const noexcept;
};

}// namespace luisa::compute::llvm
