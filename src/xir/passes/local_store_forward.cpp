#include <luisa/core/logging.h>
#include <luisa/xir/function.h>
#include <luisa/xir/module.h>
#include <luisa/xir/builder.h>

#include "helpers.h"
#include <luisa/xir/passes/local_store_forward.h>

namespace luisa::compute::xir {

namespace detail {

// TODO: we only handle local alloca's in straight-line code for now
static void run_local_store_forward_on_basic_block(luisa::unordered_set<BasicBlock *> &visited,
                                                   BasicBlock *block,
                                                   LocalStoreForwardInfo &info) noexcept {

    luisa::unordered_map<AllocaInst *, luisa::vector<Value *>> variable_pointers;// maps variables to pointers
    luisa::unordered_map<Value *, StoreInst *> latest_stores;                    // maps pointers to the latest store instruction
    luisa::unordered_map<LoadInst *, StoreInst *> removable_loads;               // maps loads to the store that can be forwarded

    auto invalidate_interfering_stores = [&](Value *ptr) noexcept -> AllocaInst * {
        if (auto alloca_inst = trace_pointer_base_local_alloca_inst(ptr)) {
            auto &interfering_ptrs = variable_pointers[alloca_inst];
            interfering_ptrs.emplace_back(ptr);
            for (auto interfering_ptr : interfering_ptrs) {
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

    // perform the forwarding
    for (auto [load, store] : removable_loads) {
        load->replace_all_uses_with(store->value());
        load->remove_self();
        info.forwarded_instructions.emplace(load, store);
    }
}

// forward stores to loads within straight-line code
static void forward_straight_line_stores_to_loads_on_function(FunctionDefinition *function, LocalStoreForwardInfo &info) noexcept {
    luisa::unordered_set<BasicBlock *> visited;
    function->traverse_basic_blocks(BasicBlockTraversalOrder::REVERSE_POST_ORDER, [&](BasicBlock *block) noexcept {
        run_local_store_forward_on_basic_block(visited, block, info);
    });
}

// find and remove all loads from local variables that only have a single (or no) store
static void forward_single_store_to_loads_on_function(FunctionDefinition *function, LocalStoreForwardInfo &info) noexcept {
    luisa::unordered_map<AllocaInst *, StoreInst *> single_store;
    // search for local variables that only have a single store
    {
        luisa::unordered_map<AllocaInst *, size_t> store_count;
        function->traverse_instructions([&](Instruction *inst) noexcept {
            switch (inst->derived_instruction_tag()) {
                case DerivedInstructionTag::LOAD: [[fallthrough]];
                case DerivedInstructionTag::GEP: break;
                default: {
                    for (auto op_use : inst->operand_uses()) {
                        if (auto base_alloca = trace_pointer_base_local_alloca_inst(op_use->value())) {
                            store_count.try_emplace(base_alloca, 0u).first->second++;
                        }
                    }
                    break;
                }
            }
        });
        for (auto [alloca_inst, count] : store_count) {
            if (count == 1u) {
                for (auto &&use : alloca_inst->use_list()) {
                    if (auto user = use.user();
                        user->derived_value_tag() == DerivedValueTag::INSTRUCTION &&
                        static_cast<Instruction *>(user)->derived_instruction_tag() == DerivedInstructionTag::STORE) {
                        auto store_inst = static_cast<StoreInst *>(user);
                        LUISA_DEBUG_ASSERT(store_inst->variable() == alloca_inst, "Store variable must match alloca.");
                        single_store.emplace(alloca_inst, store_inst);
                        break;
                    }
                }
            }
        }
    }
    // collect the loads that might be eliminated
    luisa::vector<LoadInst *> removable_loads;
    function->traverse_instructions([&](Instruction *inst) noexcept {
        if (inst->derived_instruction_tag() == DerivedInstructionTag::LOAD) {
            auto load = static_cast<LoadInst *>(inst);
            if (auto base_alloca = trace_pointer_base_local_alloca_inst(load->variable());
                base_alloca != nullptr && single_store.contains(base_alloca)) {
                removable_loads.emplace_back(load);
            }
        }
    });
    // do the elimination
    for (auto load : removable_loads) {
        // convert load to extract
        luisa::fixed_vector<Value *, 8u> extract_args;
        LUISA_DEBUG_ASSERT(load->variable()->derived_value_tag() == DerivedValueTag::INSTRUCTION,
                           "Load variable must be an instruction.");
        auto pointer = static_cast<Instruction *>(load->variable());
        for (;;) {
            if (auto tag = pointer->derived_instruction_tag(); tag == DerivedInstructionTag::ALLOCA) {
                break;
            } else if (tag == DerivedInstructionTag::GEP) {
                auto gep = static_cast<GEPInst *>(pointer);
                LUISA_DEBUG_ASSERT(gep->base()->derived_value_tag() == DerivedValueTag::INSTRUCTION,
                                   "GEP base must be an instruction.");
                auto sub_indices = gep->index_uses();
                // note: we emplace the indices in reverse order to avoid
                // expensive insertions at the beginning of the vector
                for (auto iter = sub_indices.rbegin(); iter != sub_indices.rend(); ++iter) {
                    extract_args.emplace_back((*iter)->value());
                }
                pointer = static_cast<Instruction *>(gep->base());
            } else {
                LUISA_ERROR_WITH_LOCATION("Unexpected instruction type.");
            }
        }
        // process the alloca pointer
        LUISA_DEBUG_ASSERT(pointer->derived_instruction_tag() == DerivedInstructionTag::ALLOCA,
                           "Pointer must be an alloca.");
        auto store = single_store[static_cast<AllocaInst *>(pointer)];
        LUISA_DEBUG_ASSERT(store != nullptr, "Store must not be null.");
        extract_args.emplace_back(store->value());
        auto value = [&]() noexcept -> Value * {
            // simple case: scalar load
            if (extract_args.size() == 1u) { return extract_args.front(); }
            // reverse the indices to the correct order
            std::reverse(extract_args.begin(), extract_args.end());
            // create the extract instruction
            Builder builder;
            builder.set_insertion_point(load);
            return builder.call(load->type(), ArithmeticOp::EXTRACT, extract_args);
        }();
        load->replace_all_uses_with(value);
        load->remove_self();
        // record the elimination
        info.forwarded_instructions.emplace(load, store);
    }
}

static void run_local_store_forward_on_function(Function *function, LocalStoreForwardInfo &info) noexcept {
    if (auto definition = function->definition()) {
        // first pass: forward stores to loads within straight-line code
        forward_straight_line_stores_to_loads_on_function(definition, info);
        // second pass: forward stores to loads from local variables that only have a single (or no) store
        forward_single_store_to_loads_on_function(definition, info);
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
