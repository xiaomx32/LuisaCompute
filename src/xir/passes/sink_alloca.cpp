#include <luisa/core/logging.h>
#include <luisa/core/stl/unordered_map.h>
#include <luisa/xir/function.h>
#include <luisa/xir/instructions/alloca.h>
#include <luisa/xir/passes/sink_alloca.h>

namespace luisa::compute::xir {

namespace detail {

[[nodiscard]] BasicBlock *find_common_dominator_block(BasicBlock *current, luisa::span<BasicBlock *const> blocks) noexcept {
    // if some block is just the current block, then it must be the common dominator
    if (std::binary_search(blocks.begin(), blocks.end(), current)) { return current; }
    // otherwise we have to recursively find the common dominator of the dominators
    LUISA_NOT_IMPLEMENTED();
    return nullptr;
}

[[nodiscard]] bool try_sink_alloca(FunctionDefinition *def, AllocaInst *alloca) noexcept {
    luisa::fixed_vector<BasicBlock *, 64u> user_blocks;
    luisa::fixed_vector<Instruction *, 64u> user_instructions;
    // collect user blocks
    for (auto &&use : alloca->use_list()) {
        if (auto user = use.user()) {
            // we only handle instruction users for now
            if (user->derived_value_tag() != DerivedValueTag::INSTRUCTION) {
                return false;
            }
            auto user_inst = static_cast<Instruction *>(user);
            LUISA_DEBUG_ASSERT(user_inst->parent_block() != nullptr,
                               "Instruction must have a parent block.");
            user_blocks.emplace_back(user_inst->parent_block());
            user_instructions.emplace_back(user_inst);
        }
    }
    // sort and unique user blocks
    std::sort(user_blocks.begin(), user_blocks.end());
    user_blocks.erase(std::unique(user_blocks.begin(), user_blocks.end()), user_blocks.end());
    // find the common dominator block of all user blocks
    auto common_dominator = find_common_dominator_block(def->body_block(), user_blocks);
    // sort and unique user instructions
    std::sort(user_instructions.begin(), user_instructions.end());
    user_instructions.erase(std::unique(user_instructions.begin(), user_instructions.end()), user_instructions.end());
    // find in the dominator block the first instruction that uses the alloca (or the terminator)
    auto first_user_inst = [&]() noexcept -> Instruction * {
        for (auto &&inst : common_dominator->instructions()) {
            if (std::binary_search(user_instructions.begin(), user_instructions.end(), &inst)) {
                return &inst;
            }
        }
        return common_dominator->terminator();
    }();
    // move the alloca if the position would change
    if (alloca->next() == first_user_inst) {
        return false;
    }
    alloca->remove_self();
    first_user_inst->insert_before_self(alloca);
    return true;
}

void run_sink_alloca_pass_on_function(Function *f, SinkAllocaInfo &info) noexcept {
    if (auto def = f->definition()) {
        luisa::vector<AllocaInst *> collected;
        def->traverse_instructions([&](Instruction *inst) noexcept {
            if (inst->derived_instruction_tag() == DerivedInstructionTag::ALLOCA) {
                collected.emplace_back(static_cast<AllocaInst *>(inst));
            }
        });
        for (auto alloca : collected) {
            if (try_sink_alloca(def, alloca)) {
                info.sunken_instructions.emplace_back(alloca);
            }
        }
    }
}

}// namespace detail

SinkAllocaInfo sink_alloca_pass_run_on_function(Function *function) noexcept {
    SinkAllocaInfo info;
    detail::run_sink_alloca_pass_on_function(function, info);
    return info;
}

SinkAllocaInfo sink_alloca_pass_run_on_module(Module *module) noexcept {
    SinkAllocaInfo info;
    for (auto &f : module->functions()) {
        detail::run_sink_alloca_pass_on_function(&f, info);
    }
    return info;
}

}// namespace luisa::compute::xir
