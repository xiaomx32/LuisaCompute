#include <luisa/xir/basic_block.h>

namespace luisa::compute::xir {

BasicBlock::BasicBlock(Pool *pool) noexcept
    : DerivedValue{pool, nullptr}, _instructions{pool} {
    _instructions.head_sentinel()->_set_parent_block(this);
    _instructions.tail_sentinel()->_set_parent_block(this);
}

}// namespace luisa::compute::xir
