#include <luisa/luisa-compute.h>

using namespace luisa;
using namespace luisa::compute;

int main() {
    auto module = xir::Module{};
    auto kernel = module.create_kernel();
    auto x = kernel->create_value_argument(Type::of<int>());
    auto y = kernel->create_value_argument(Type::of<int>());

    auto b = xir::Builder{};
    b.set_insertion_point(kernel->body());
    auto z = b.call(Type::of<int>(), xir::IntrinsicOp::BINARY_ADD, {x, y});
    b.return_(z);

}
