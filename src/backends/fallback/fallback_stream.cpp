//
// Created by Mike Smith on 2022/2/7.
//

#include <algorithm>
#include "fallback_stream.h"
#include "fallback_accel.h"
#include "fallback_bindless_array.h"
#include "fallback_mesh.h"
#include "fallback_texture.h"
#include "fallback_shader.h"
#include "fallback_buffer.h"
#include "luisa/core/logging.h"

namespace luisa::compute::fallback {

using std::max;

void FallbackStream::dispatch(CommandList &&cmd_list) noexcept {

    auto cmds = cmd_list.steal_commands();

    for (auto &&cmd : cmds) {
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
    LUISA_NOT_IMPLEMENTED();
    //    event->signal(_pool.async([] {}));
}

void FallbackStream::wait(LLVMEvent *event) noexcept {
    LUISA_NOT_IMPLEMENTED();
    //    _pool.async([future = event->future()] { future.wait(); });
    //    _pool.barrier();
}

void FallbackStream::visit(BufferUploadCommand *command) noexcept {
    auto temp_buffer = luisa::allocate_with_allocator<std::byte>(command->size());
    std::memcpy(temp_buffer, command->data(), command->size());
    auto dst = reinterpret_cast<FallbackBuffer *>(command->handle())->view(command->offset(), command->size());
    _pool.async([src = temp_buffer, dst] {
        std::memcpy(dst.ptr, src, dst.size);
        luisa::deallocate_with_allocator(src);
    });
    _pool.barrier();
}

void FallbackStream::visit(BufferDownloadCommand *command) noexcept {
    auto src = reinterpret_cast<FallbackBuffer *>(command->handle())->view(command->offset(), command->size());
    _pool.async([dst = command->data(), src] { std::memcpy(dst, src.ptr, src.size); });
    _pool.barrier();
}

void FallbackStream::visit(BufferCopyCommand *command) noexcept {
    auto src = reinterpret_cast<FallbackBuffer *>(command->src_handle())->view(command->src_offset(), command->size());
    auto dst = reinterpret_cast<FallbackBuffer *>(command->dst_handle())->view(command->dst_offset(), command->size());
    _pool.async([src, dst = dst.ptr] { std::memcpy(dst, src.ptr, src.size); });
    _pool.barrier();
}

void FallbackStream::visit(BufferToTextureCopyCommand *command) noexcept {
    LUISA_NOT_IMPLEMENTED();
    //    _pool.async([cmd = *command] {
    //        auto src = reinterpret_cast<const void *>(cmd.buffer() + cmd.buffer_offset());
    //        auto tex = reinterpret_cast<FallbackTexture *>(cmd.texture())->view(cmd.level());
    //        tex.copy_from(src);
    //    });
}

void FallbackStream::visit(ShaderDispatchCommand *command) noexcept {
    auto shader = reinterpret_cast<const FallbackShader *>(command->handle());
    shader->dispatch(_pool, command);
    _pool.barrier();
}

void FallbackStream::visit(TextureUploadCommand *command) noexcept {
    auto tex = reinterpret_cast<FallbackTexture *>(command->handle())->view(command->level());
    auto byte_size = pixel_storage_size(tex.storage(), tex.size3d());
    auto temp_buffer = luisa::allocate_with_allocator<std::byte>(byte_size);
    std::memcpy(temp_buffer, command->data(), byte_size);
    _pool.async([tex, temp_buffer] {
        auto byte_size = pixel_storage_size(tex.storage(), tex.size3d());
        std::memcpy(const_cast<std::byte *>(tex.data()), temp_buffer, byte_size);
        luisa::deallocate_with_allocator(temp_buffer);
    });
    _pool.barrier();
}

void FallbackStream::visit(TextureDownloadCommand *command) noexcept {
    auto tex = reinterpret_cast<FallbackTexture *>(command->handle())->view(command->level());
    _pool.async([=, dst = command->data()] { tex.copy_to(dst); });
    _pool.barrier();
}

void FallbackStream::visit(TextureCopyCommand *command) noexcept {
    auto src_tex = reinterpret_cast<FallbackTexture *>(command->src_handle())->view(command->src_level());
    auto dst_tex = reinterpret_cast<FallbackTexture *>(command->dst_handle())->view(command->dst_level());
    _pool.async([=] { dst_tex.copy_from(src_tex); });
    _pool.barrier();
}

void FallbackStream::visit(TextureToBufferCopyCommand *command) noexcept {
    auto src = reinterpret_cast<FallbackTexture *>(command->texture())->view(command->level());
    auto dst = reinterpret_cast<FallbackBuffer *>(command->buffer())->view_with_offset(command->buffer_offset());
    _pool.async([src, dst = dst.ptr] { src.copy_to(dst); });
    _pool.barrier();
}

void FallbackStream::visit(AccelBuildCommand *command) noexcept {
    auto accel = reinterpret_cast<FallbackAccel *>(command->handle());
    accel->build(_pool, command->instance_count(), command->steal_modifications());
    _pool.barrier();
}

void FallbackStream::visit(MeshBuildCommand *command) noexcept {
    auto v_b = reinterpret_cast<FallbackBuffer *>(command->vertex_buffer())->data();
    auto v_b_o = command->vertex_buffer_offset();
    auto v_s = command->vertex_stride();
    auto v_b_s = command->vertex_buffer_size();
    auto v_b_c = v_b_s / v_s;
    auto t_b = reinterpret_cast<FallbackBuffer *>(command->triangle_buffer())->data();
    auto t_b_o = command->triangle_buffer_offset();
    auto t_b_s = command->triangle_buffer_size();
    auto t_b_c = t_b_s / 12u;
    _pool.async([=, mesh = reinterpret_cast<FallbackMesh *>(command->handle())] {
        mesh->commit(reinterpret_cast<uint64_t>(v_b), v_b_o, v_s, v_b_c,
                     reinterpret_cast<uint64_t>(t_b), t_b_o, t_b_c);
    });
    _pool.barrier();
}

void FallbackStream::visit(BindlessArrayUpdateCommand *command) noexcept {
    reinterpret_cast<FallbackBindlessArray *>(command->handle())->update(_pool, command->steal_modifications());
    _pool.barrier();
}

void FallbackStream::dispatch(luisa::move_only_function<void()> &&f) noexcept {
    LUISA_NOT_IMPLEMENTED();
    //    auto ptr = new_with_allocator<luisa::move_only_function<void()>>(std::move(f));
    //    _pool.async([ptr] {
    //        (*ptr)();
    //        delete_with_allocator(ptr);
    //    });
    //    _pool.barrier();
}

FallbackStream::FallbackStream() noexcept = default;

void FallbackStream::visit(CurveBuildCommand *command) noexcept {
    LUISA_NOT_IMPLEMENTED();
}

void FallbackStream::visit(ProceduralPrimitiveBuildCommand *command) noexcept {
    LUISA_NOT_IMPLEMENTED();
}

void FallbackStream::visit(MotionInstanceBuildCommand *command) noexcept {
    LUISA_NOT_IMPLEMENTED();
}

void FallbackStream::visit(CustomCommand *command) noexcept {
    LUISA_NOT_IMPLEMENTED();
}

}// namespace luisa::compute::fallback
