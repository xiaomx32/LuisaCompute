#include <luisa/xir/function.h>
#include <luisa/xir/module.h>
#include <luisa/xir/builder.h>
#include <luisa/xir/passes/peephole_store_forward.h>

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

// TODO: we only handle scalars for now
static void run_peephole_store_forward_on_basic_block(BasicBlock *block, PeepholeStoreForwardInfo &info) noexcept {

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
    for (auto &&[load, store] : removable_loads) {
        load->replace_all_uses_with(store->value());
        load->remove_self();
        info.forwarded_instructions.emplace(load, store);
    }
}

void run_peephole_store_forward_on_function(Function *function, PeepholeStoreForwardInfo &info) noexcept {
    if (auto definition = function->definition()) {
        definition->traverse_basic_blocks([&](BasicBlock *block) noexcept {
            run_peephole_store_forward_on_basic_block(block, info);
        });
    }
}

}// namespace detail

PeepholeStoreForwardInfo peephole_store_forward_pass_run_on_basic_block(BasicBlock *block) noexcept {
    PeepholeStoreForwardInfo info;
    detail::run_peephole_store_forward_on_basic_block(block, info);
    return info;
}

PeepholeStoreForwardInfo peephole_store_forward_pass_run_on_function(Function *function) noexcept {
    PeepholeStoreForwardInfo info;
    detail::run_peephole_store_forward_on_function(function, info);
    return info;
}

PeepholeStoreForwardInfo peephole_store_forward_pass_run_on_module(Module *module) noexcept {
    PeepholeStoreForwardInfo info;
    for (auto &&f : module->functions()) {
        detail::run_peephole_store_forward_on_function(&f, info);
    }
    return info;
}

}// namespace luisa::compute::xir
