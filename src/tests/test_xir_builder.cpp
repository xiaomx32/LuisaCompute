#include <luisa/luisa-compute.h>

using namespace luisa;
using namespace luisa::compute;

int main() {
    auto module = xir::Module{};
    auto f = module.create_callable(Type::of<float>());
    auto x = f->create_value_argument(Type::of<float>());
    auto y = f->create_value_argument(Type::of<float>());

    auto b = xir::Builder{};
    b.set_insertion_point(f->body());
    auto add = b.call(Type::of<float>(), xir::IntrinsicOp::BINARY_MUL, {x, y});
    auto mul = b.call(Type::of<float>(), xir::IntrinsicOp::BINARY_ADD, {add, y});
    b.return_(mul);
}
