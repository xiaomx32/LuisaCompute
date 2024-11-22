//
// Created by Mike Smith on 2022/2/7.
//

#include <algorithm>
#include "fallback_stream.h"
#include "fallback_accel.h"
#include "fallback_mesh.h"
#include "fallback_texture.h"
#include "fallback_shader.h"

namespace luisa::compute::fallback {

using std::max;

void FallbackStream::dispatch(CommandList &&cmd_list) noexcept {

    auto cmds = cmd_list.steal_commands();

    for (auto &&cmd : cmds)
    {
        for (;;) {
            auto n = _pool.task_count();
            if (n < _pool.size() * 4u) { break; }
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(50us);
        }
        cmd->accept(*this);
    }
    _pool.barrier();
}

void FallbackStream::signal(LLVMEvent *event) noexcept {
//    event->signal(_pool.async([] {}));
}

void FallbackStream::wait(LLVMEvent *event) noexcept {
//    _pool.async([future = event->future()] { future.wait(); });
//    _pool.barrier();
}

void FallbackStream::visit(const BufferUploadCommand *command) noexcept {
    auto temp_buffer = luisa::make_shared<luisa::vector<std::byte>>(command->size());
    std::memcpy(temp_buffer->data(), command->data(), command->size());
    _pool.async([src = std::move(temp_buffer),
                 buffer = command->handle(), offset = command->offset()] {
        auto dst = reinterpret_cast<void *>(buffer + offset);
        std::memcpy(dst, src->data(), src->size());
    });
}

void FallbackStream::visit(const BufferDownloadCommand *command) noexcept {
    _pool.async([cmd = *command] {
        auto src = reinterpret_cast<const void *>(cmd.handle() + cmd.offset());
        std::memcpy(cmd.data(), src, cmd.size());
    });
}

void FallbackStream::visit(const BufferCopyCommand *command) noexcept {
    _pool.async([cmd = *command] {
        auto src = reinterpret_cast<const void *>(cmd.src_handle() + cmd.src_offset());
        auto dst = reinterpret_cast<void *>(cmd.dst_handle() + cmd.dst_offset());
        std::memcpy(dst, src, cmd.size());
    });
}

void FallbackStream::visit(const BufferToTextureCopyCommand *command) noexcept {
//    _pool.async([cmd = *command] {
//        auto src = reinterpret_cast<const void *>(cmd.buffer() + cmd.buffer_offset());
//        auto tex = reinterpret_cast<FallbackTexture *>(cmd.texture())->view(cmd.level());
//        tex.copy_from(src);
//    });
}

//extern "C" void llvm_callback_dispatch(const CpuCallback *callbacks, void *arg, void *user_data, uint32_t callback_id) noexcept {
//    callbacks[callback_id].callback(arg, user_data);
//}

void FallbackStream::visit(const ShaderDispatchCommand *command) noexcept
{
    auto shader = reinterpret_cast<const FallbackShader *>(command->handle());
    shader->dispatch(_pool, command);
}

void FallbackStream::visit(const TextureUploadCommand *command) noexcept {
    auto byte_size = command->size().x * command->size().y * command->size().z *
                     pixel_storage_align(command->storage());
    auto temp_buffer = luisa::make_shared<luisa::vector<std::byte>>(byte_size);
    std::memcpy(temp_buffer->data(), command->data(), byte_size);
    _pool.async([cmd = *command, temp_buffer = std::move(temp_buffer)] {
        auto tex = reinterpret_cast<FallbackTexture *>(cmd.handle())->view(cmd.level());
        tex.copy_from(temp_buffer->data());
    });
}

void FallbackStream::visit(const TextureDownloadCommand *command) noexcept {
    _pool.async([cmd = *command] {
        auto tex = reinterpret_cast<FallbackTexture *>(cmd.handle())->view(cmd.level());
        tex.copy_to(cmd.data());
    });
}

void FallbackStream::visit(const TextureCopyCommand *command) noexcept {
    _pool.async([cmd = *command] {
        auto src_tex = reinterpret_cast<FallbackTexture *>(cmd.src_handle())->view(cmd.src_level());
        auto dst_tex = reinterpret_cast<FallbackTexture *>(cmd.dst_handle())->view(cmd.dst_level());
        dst_tex.copy_from(src_tex);
    });
}

void FallbackStream::visit(const TextureToBufferCopyCommand *command) noexcept {
    _pool.async([cmd = *command] {
        auto tex = reinterpret_cast<FallbackTexture *>(cmd.texture())->view(cmd.level());
        auto dst = reinterpret_cast<void *>(cmd.buffer() + cmd.buffer_offset());
        tex.copy_to(dst);
    });
}

void FallbackStream::visit(const AccelBuildCommand *command) noexcept {
    auto accel = reinterpret_cast<FallbackAccel *>(command->handle());
    accel->build(_pool, command->instance_count(), command->modifications());
}

void FallbackStream::visit(const MeshBuildCommand *command) noexcept {
    auto v_b = command->vertex_buffer();
    auto v_b_o = command->vertex_buffer_offset();
    auto v_s = command->vertex_stride();
    auto v_b_s = command->vertex_buffer_size();
    auto t_b = command->triangle_buffer();
    auto t_b_o = command->triangle_buffer_offset();
    auto t_b_s = command->triangle_buffer_size();
    _pool.async([&,mesh = reinterpret_cast<FallbackMesh *>(command->handle())]
    {
        mesh->commit(v_b, v_b_o, v_s, v_b_s, t_b, t_b_o, t_b_s);
    });
}

void FallbackStream::visit(const BindlessArrayUpdateCommand *command) noexcept {
    //reinterpret_cast<LLVMBindlessArray *>(command->handle())->update(_pool);
}

void FallbackStream::dispatch(luisa::move_only_function<void()> &&f) noexcept {
//    auto ptr = new_with_allocator<luisa::move_only_function<void()>>(std::move(f));
//    _pool.async([ptr] {
//        (*ptr)();
//        delete_with_allocator(ptr);
//    });
//    _pool.barrier();
}
FallbackStream::FallbackStream() noexcept {
}
void FallbackStream::visit(const CurveBuildCommand *command) noexcept {
}
void FallbackStream::visit(const ProceduralPrimitiveBuildCommand *command) noexcept {
}
void FallbackStream::visit(const MotionInstanceBuildCommand *command) noexcept {
}
void FallbackStream::visit(const CustomCommand *command) noexcept {
}

}// namespace luisa::compute::llvm
