#include <luisa/luisa-compute.h>

using namespace luisa;
using namespace luisa::compute;

int main() {
    auto module = xir::Module{};
    auto b = xir::Builder{};

    auto f = module.create_callable(Type::of<float>());
    auto x = f->create_value_argument(Type::of<float>());
    auto y = f->create_value_argument(Type::of<float>());
    auto ray = f->create_value_argument(Type::of<Ray>());

    b.set_insertion_point(f->body());
    auto add = b.call(Type::of<float>(), xir::IntrinsicOp::BINARY_MUL, {x, y});
    auto mul = b.call(Type::of<float>(), xir::IntrinsicOp::BINARY_ADD, {add, y});
    auto coord = b.call(Type::of<uint3>(), xir::IntrinsicOp::DISPATCH_ID, {});
    auto zero = b.const_(0u);
    auto coord_x = b.call(Type::of<uint>(), xir::IntrinsicOp::EXTRACT, {coord, zero});
    auto cond = b.call(Type::of<bool>(), xir::IntrinsicOp::BINARY_EQUAL, {coord_x, zero});
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
    auto one = b.const_(1.0f);
    b.store(va, one);
    b.store(vb, one);
    auto result = b.call(Type::of<float>(), f, {va, vb});
    b.call(nullptr, xir::IntrinsicOp::BUFFER_WRITE, {buffer, zero, result});
    b.return_void();

    LUISA_INFO("IR:\n{}", xir::translate_to_text(module));
}
