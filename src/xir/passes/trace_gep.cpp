#include <luisa/xir/function.h>
#include <luisa/xir/instructions/gep.h>
#include <luisa/xir/passes/trace_gep.h>

namespace luisa::compute::xir {

namespace detail {

[[nodiscard]] static bool value_is_gep(Value *value) noexcept {
    return value->derived_value_tag() == DerivedValueTag::INSTRUCTION &&
           static_cast<Instruction *>(value)->derived_instruction_tag() == DerivedInstructionTag::GEP;
}

[[nodiscard]] static Value *collect_gep_indices_recursive(GEPInst *inst, luisa::vector<Value *> &indices) noexcept {
    auto origin = inst->base();
    if (value_is_gep(origin)) {
        origin = collect_gep_indices_recursive(static_cast<GEPInst *>(origin), indices);
    }
    for (auto i : inst->index_uses()) {
        indices.emplace_back(i->value());
    }
    return origin;
}

[[nodiscard]] static bool try_trace_gep_inst(GEPInst *inst) noexcept {
    if (!value_is_gep(inst->base())) { return false; }
    luisa::vector<Value *> indices;
    auto origin = collect_gep_indices_recursive(inst, indices);
    inst->set_operand_count(1u + indices.size());
    inst->set_operand(0, origin);
    for (auto i = 0u; i < indices.size(); i++) {
        inst->set_operand(i + 1, indices[i]);
    }
    return true;
}

static void trace_gep_instructions_in_function(Function *function, TraceGEPInfo &info) noexcept {
    if (auto definition = function->definition()) {
        definition->traverse_instructions([&](Instruction *inst) noexcept {
            if (inst->derived_instruction_tag() == DerivedInstructionTag::GEP) {
                auto gep_inst = static_cast<GEPInst *>(inst);
                if (try_trace_gep_inst(gep_inst)) {
                    info.traced_geps.emplace_back(gep_inst);
                }
            }
        });
    }
}

}// namespace detail

TraceGEPInfo trace_gep_pass_run_on_function(Function *function) noexcept {
    TraceGEPInfo info;
    detail::trace_gep_instructions_in_function(function, info);
    return info;
}

TraceGEPInfo trace_gep_pass_run_on_module(Module *module) noexcept {
    TraceGEPInfo info;
    for (auto &f : module->functions()) {
        detail::trace_gep_instructions_in_function(&f, info);
    }
    return info;
}

}// namespace luisa::compute::xir
