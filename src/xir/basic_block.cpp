#include <luisa/xir/basic_block.h>

namespace luisa::compute::xir {

BasicBlock::BasicBlock(Pool *pool) noexcept
    : DerivedValue{pool, nullptr}, _instructions{pool} {
    _instructions.head_sentinel()->_set_parent_block(this);
    _instructions.tail_sentinel()->_set_parent_block(this);
}

void BasicBlock::_set_parent_value(Value *parent_value) noexcept {
    _parent_value = parent_value;
}

}// namespace luisa::compute::xir
