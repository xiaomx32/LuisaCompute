#include "register.h"
#include "string_builder.h"
#include <luisa/core/stl/format.h>

namespace lc::spirv {
Register::Register() = default;
Register::Register(uint64_t id) : _id{id} {}
namespace register_detail {
static thread_local uint64_t counter = 0;
}// namespace register_detail
void Register::reset_register() {
    register_detail::counter = 0;
}

Register Register::new_register() {
    return Register{++register_detail::counter};
}
luisa::string Register::to_str() const {
    return luisa::format("%{}", _id);
}
}// namespace lc::spirv