//
// Created by Mike Smith on 2022/2/7.
//

#pragma once

#include "thread_pool.h"
#include <luisa/runtime/command_list.h>

namespace luisa::compute::fallback {

class LLVMEvent;

/**
 * @brief Stream of LLVM
 * 
 */
class FallbackStream final : public MutableCommandVisitor {

private:
    ThreadPool _pool;

public:
    FallbackStream() noexcept;
    [[nodiscard]] auto pool() noexcept { return &_pool; }
    void synchronize() noexcept { _pool.synchronize(); }
    void dispatch(CommandList &&cmd_list) noexcept;
    void dispatch(luisa::move_only_function<void()> &&f) noexcept;
    void signal(LLVMEvent *event) noexcept;
    void wait(LLVMEvent *event) noexcept;

public:
    void visit(BufferUploadCommand *command) noexcept override;
    void visit(BufferDownloadCommand *command) noexcept override;
    void visit(BufferCopyCommand *command) noexcept override;
    void visit(BufferToTextureCopyCommand *command) noexcept override;
    void visit(ShaderDispatchCommand *command) noexcept override;
    void visit(TextureUploadCommand *command) noexcept override;
    void visit(TextureDownloadCommand *command) noexcept override;
    void visit(TextureCopyCommand *command) noexcept override;
    void visit(TextureToBufferCopyCommand *command) noexcept override;
    void visit(AccelBuildCommand *command) noexcept override;
    void visit(MeshBuildCommand *command) noexcept override;
    void visit(BindlessArrayUpdateCommand *command) noexcept override;
    void visit(CurveBuildCommand *command) noexcept override;
    void visit(ProceduralPrimitiveBuildCommand *command) noexcept override;
    void visit(MotionInstanceBuildCommand *command) noexcept override;
    void visit(CustomCommand *command) noexcept override;
    ~FallbackStream() noexcept override = default;
};

}// namespace luisa::compute::fallback
