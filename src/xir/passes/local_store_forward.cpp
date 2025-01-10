#include <luisa/xir/function.h>
#include <luisa/xir/module.h>
#include <luisa/xir/builder.h>

#include <luisa/xir/passes/local_store_forward.h>

namespace luisa::compute::xir {

namespace detail {

[[nodiscard]] AllocaInst *trace_pointer_base_local_alloca_inst(Value *pointer) noexcept {
    if (pointer == nullptr || pointer->derived_value_tag() != DerivedValueTag::INSTRUCTION) {
        return nullptr;
    }
    switch (auto inst = static_cast<Instruction *>(pointer); inst->derived_instruction_tag()) {
        case DerivedInstructionTag::ALLOCA: {
            if (auto alloca_inst = static_cast<AllocaInst *>(inst); alloca_inst->space() == AllocSpace::LOCAL) {
                return alloca_inst;
            }
            return nullptr;
        }
        case DerivedInstructionTag::GEP: {
            auto gep_inst = static_cast<GEPInst *>(inst);
            return trace_pointer_base_local_alloca_inst(gep_inst->base());
        }
        default: break;
    }
    return nullptr;
}

// TODO: we only handle local alloca's in straight-line code for now
static void run_local_store_forward_on_basic_block(luisa::unordered_set<BasicBlock *> &visited,
                                                   BasicBlock *block, LocalStoreForwardInfo &info) noexcept {

    luisa::unordered_map<AllocaInst *, luisa::vector<Value *>> variable_pointers;// maps variables to pointers
    luisa::unordered_map<Value *, StoreInst *> latest_stores;                    // maps pointers to the latest store instruction
    luisa::unordered_map<LoadInst *, StoreInst *> removable_loads;               // maps loads to the store that can be forwarded

    auto invalidate_interfering_stores = [&](Value *ptr) noexcept -> AllocaInst * {
        if (auto alloca_inst = trace_pointer_base_local_alloca_inst(ptr)) {
            variable_pointers[alloca_inst].emplace_back(ptr);
            for (auto interfering_ptr : variable_pointers[alloca_inst]) {
                latest_stores.erase(interfering_ptr);
            }
            return alloca_inst;
        }
        return nullptr;
    };

    // we visit the block and all of its single straight-line successors
    while (visited.emplace(block).second) {
        // process the instructions in the block
        for (auto &&inst : block->instructions()) {
            switch (inst.derived_instruction_tag()) {
                case DerivedInstructionTag::LOAD: {
                    auto load = static_cast<LoadInst *>(&inst);
                    if (auto iter = latest_stores.find(load->variable()); iter != latest_stores.end()) {
                        removable_loads.emplace(load, iter->second);
                    }
                    break;
                }
                case DerivedInstructionTag::STORE: {
                    auto store = static_cast<StoreInst *>(&inst);
                    // if this is a store to (part of) a local alloca, we might be able to forward it
                    if (auto pointer = store->variable(); invalidate_interfering_stores(pointer)) {
                        latest_stores[pointer] = store;
                    }
                    break;
                }
                case DerivedInstructionTag::GEP: {
                    // users of GEPs will handle the forwarding, so we don't need to do anything here
                    break;
                }
                default: {// for other instructions, we invalidate possibly interfering stores
                    for (auto op_use : inst.operand_uses()) {
                        invalidate_interfering_stores(op_use->value());
                    }
                    break;
                }
            }
        }
        // move to the next block if it is the only successor and only has a single predecessor
        BasicBlock *next = nullptr;
        auto successor_count = 0u;
        block->traverse_successors(true, [&](BasicBlock *succ) noexcept {
            successor_count++;
            next = succ;
        });
        if (successor_count != 1) { break; }
        // check if the next block has a single predecessor
        auto pred_count = 0u;
        next->traverse_predecessors(false, [&](BasicBlock *) noexcept { pred_count++; });
        if (pred_count != 1) { break; }
        block = next;
    }
    for (auto &&[load, store] : removable_loads) {
        load->replace_all_uses_with(store->value());
        load->remove_self();
        info.forwarded_instructions.emplace(load, store);
    }
}

void run_local_store_forward_on_function(Function *function, LocalStoreForwardInfo &info) noexcept {
    if (auto definition = function->definition()) {
        luisa::unordered_set<BasicBlock *> visited;
        definition->traverse_basic_blocks(BasicBlockTraversalOrder::REVERSE_POST_ORDER, [&](BasicBlock *block) noexcept {
            run_local_store_forward_on_basic_block(visited, block, info);
        });
    }
}

}// namespace detail

LocalStoreForwardInfo local_store_forward_pass_run_on_function(Function *function) noexcept {
    LocalStoreForwardInfo info;
    detail::run_local_store_forward_on_function(function, info);
    return info;
}

LocalStoreForwardInfo local_store_forward_pass_run_on_module(Module *module) noexcept {
    LocalStoreForwardInfo info;
    for (auto &&f : module->functions()) {
        detail::run_local_store_forward_on_function(&f, info);
    }
    return info;
}

}// namespace luisa::compute::xir
