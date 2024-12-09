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
class FallbackStream final : public CommandVisitor {

private:
    ThreadPool _pool;

public:
    FallbackStream() noexcept;
    void synchronize() noexcept { _pool.synchronize(); }
    void dispatch(CommandList &&cmd_list) noexcept;
    void dispatch(luisa::move_only_function<void()> &&f) noexcept;
    void signal(LLVMEvent *event) noexcept;
    void wait(LLVMEvent *event) noexcept;

public:
    void visit(const BufferUploadCommand *command) noexcept override;
    void visit(const BufferDownloadCommand *command) noexcept override;
    void visit(const BufferCopyCommand *command) noexcept override;
    void visit(const BufferToTextureCopyCommand *command) noexcept override;
    void visit(const ShaderDispatchCommand *command) noexcept override;
    void visit(const TextureUploadCommand *command) noexcept override;
    void visit(const TextureDownloadCommand *command) noexcept override;
    void visit(const TextureCopyCommand *command) noexcept override;
    void visit(const TextureToBufferCopyCommand *command) noexcept override;
    void visit(const AccelBuildCommand *command) noexcept override;
    void visit(const MeshBuildCommand *command) noexcept override;
    void visit(const BindlessArrayUpdateCommand *command) noexcept override;
    void visit(const CurveBuildCommand *command) noexcept override;
    void visit(const ProceduralPrimitiveBuildCommand *command) noexcept override;
    void visit(const MotionInstanceBuildCommand *command) noexcept override;
    void visit(const CustomCommand *command) noexcept override;
    ~FallbackStream() noexcept override = default;
};

}// namespace luisa::compute::fallback
