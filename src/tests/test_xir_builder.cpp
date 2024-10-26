#include <luisa/luisa-compute.h>

using namespace luisa;
using namespace luisa::compute;

int main() {
    auto module = xir::Module{};
    auto u32_zero = module.create_constant(0u);
    auto f32_one = module.create_constant(1.f);

    auto b = xir::Builder{};

    auto f = module.create_callable(Type::of<float>());
    auto x = f->create_value_argument(Type::of<float>());
    auto y = f->create_value_argument(Type::of<float>());
    auto ray = f->create_value_argument(Type::of<Ray>());

    b.set_insertion_point(f->body());
    auto add = b.call(Type::of<float>(), xir::IntrinsicOp::BINARY_MUL, {x, y});
    auto mul = b.call(Type::of<float>(), xir::IntrinsicOp::BINARY_ADD, {add, y});
    auto coord = b.call(Type::of<uint3>(), xir::IntrinsicOp::DISPATCH_ID, {});
    auto coord_x = b.call(Type::of<uint>(), xir::IntrinsicOp::EXTRACT, {coord, u32_zero});
    auto cond = b.call(Type::of<bool>(), xir::IntrinsicOp::BINARY_EQUAL, {coord_x, u32_zero});
    auto branch = b.if_(cond);
    auto true_block = branch->create_true_block();
    b.set_insertion_point(true_block);
    b.comment("hello, world!");
    b.print("({} + {}) * {} = {}", {x, y, y, mul});
    auto merge = branch->create_merge_block();
    b.set_insertion_point(merge);
    b.return_(mul);

    auto k = module.create_kernel();
    auto buffer = k->create_value_argument(Type::of<Buffer<float>>());
    b.set_insertion_point(k->body());
    auto va = b.alloca_local(Type::of<float>());
    auto vb = b.alloca_local(Type::of<float>());
    b.store(va, f32_one);
    b.store(vb, f32_one);
    auto result = b.call(Type::of<float>(), f, {va, vb});
    b.call(nullptr, xir::IntrinsicOp::BUFFER_WRITE, {buffer, u32_zero, result});
    b.return_void();

    LUISA_INFO("IR:\n{}", xir::translate_to_text(module));
}
