#include <luisa/core/logging.h>
#include <luisa/xir/basic_block.h>

namespace luisa::compute::xir {

BasicBlock::BasicBlock() noexcept
    : DerivedValue{nullptr} {
    _instructions.head_sentinel()->_set_parent_block(this);
    _instructions.tail_sentinel()->_set_parent_block(this);
}

bool BasicBlock::is_terminated() const noexcept {
    return _instructions.back().is_terminator();
}

TerminatorInstruction *BasicBlock::terminator() noexcept {
    LUISA_DEBUG_ASSERT(is_terminated(), "Basic block is not terminated.");
    return static_cast<TerminatorInstruction *>(&_instructions.back());
}

const TerminatorInstruction *BasicBlock::terminator() const noexcept {
    return const_cast<BasicBlock *>(this)->terminator();
}

}// namespace luisa::compute::xir
