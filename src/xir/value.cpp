#include <luisa/core/logging.h>
#include <luisa/xir/use.h>
#include <luisa/xir/value.h>

namespace luisa::compute::xir {

Value::Value(Pool *pool, const Type *type, const Name *name) noexcept
    : PooledObject{pool}, _type{type}, _name{name} {}

void Value::replace_all_uses_with(Value *value) noexcept {
    luisa::fixed_vector<Use *, 16u> uses;
    for (auto &&use : _use_list) {
        LUISA_DEBUG_ASSERT(use.value() == this,
                           "Use value mismatch.");
        uses.emplace_back(&use);
    }
    for (auto &&use : uses) {
        use->set_value(value, true);
    }
}

void Value::set_type(const Type *type) noexcept { _type = type; }
void Value::set_name(const Name *name) noexcept { _name = name; }

}// namespace luisa::compute::xir
