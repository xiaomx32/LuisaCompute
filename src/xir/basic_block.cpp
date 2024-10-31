#include <luisa/xir/basic_block.h>

namespace luisa::compute::xir {

BasicBlock::BasicBlock(Pool *pool) noexcept
    : DerivedValue{pool, nullptr}, _instructions{pool} {}

}// namespace luisa::compute::xir
