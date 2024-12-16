#include <luisa/xir/passes/outline.h>

namespace luisa::compute::xir {

OutlineInfo outline_pass_run_on_function(Module *module, Function *function) noexcept {
    return {};
}

OutlineInfo outline_pass_run_on_module(Module *module) noexcept {
    OutlineInfo info;
    luisa::vector<Function *> functions;
    for (auto &f : module->functions()) { functions.emplace_back(&f); }
    for (auto &f : module->functions()) {
        auto func_info = outline_pass_run_on_function(module, &f);

    }
    return info;
}

}
