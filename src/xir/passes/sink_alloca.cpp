#include <luisa/core/logging.h>
#include <luisa/xir/function.h>
#include <luisa/xir/passes/sink_alloca.h>

namespace luisa::compute::xir {

class AllocaInstSinker {

private:
    static void _collect_alloca_insts_in_block(luisa::vector<AllocaInst *> collected, BasicBlock *bb) noexcept {
    }

private:
    [[nodiscard]] static bool _try_sink_alloca_inst(AllocaInst *inst) noexcept {
    }

public:
    [[nodiscard]] static SinkAllocaInfo run(FunctionDefinition *f) noexcept {
        luisa::vector<AllocaInst *> collected;
        _collect_alloca_insts_in_block(collected, f->body_block());
        collected.erase(std::remove_if(collected.begin(), collected.end(), [](AllocaInst *inst) noexcept {
                            return !_try_sink_alloca_inst(inst);
                        }),
                        collected.end());
        return {std::move(collected)};
    }
};

SinkAllocaInfo sink_alloca_pass_run_on_function(Module *module, Function *function) noexcept {
    return function->is_definition() ?
               AllocaInstSinker::run(static_cast<FunctionDefinition *>(function)) :
               SinkAllocaInfo{};
}

void merge_sink_alloca_info(SinkAllocaInfo &result, const SinkAllocaInfo &info) noexcept {
    result.sunken_alloca_insts.reserve(result.sunken_alloca_insts.size() + info.sunken_alloca_insts.size());
    for (auto inst : info.sunken_alloca_insts) {
        result.sunken_alloca_insts.emplace_back(inst);
    }
}

SinkAllocaInfo sink_alloca_pass_run_on_module(Module *module) noexcept {
    SinkAllocaInfo info;
    for (auto &f : module->functions()) {
        if (f.is_definition()) {
            auto func_info = sink_alloca_pass_run_on_function(module, &f);
            merge_sink_alloca_info(info, func_info);
        }
    }
    return info;
}

}// namespace luisa::compute::xir
