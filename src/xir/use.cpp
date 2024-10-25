#include <luisa/core/logging.h>
#include <luisa/xir/value.h>
#include <luisa/xir/use.h>

namespace luisa::compute::xir {

Use::Use(Pool *pool, User *user, Value *value) noexcept
    : Super{pool}, _user{user}, _value{value} {
    LUISA_DEBUG_ASSERT(user != nullptr, "User must not be null.");
    set_value(value);
}

void Use::set_value(Value *value) noexcept {
    if (_value != value) {// we don't need to do anything if the value is not changed
        if (_value) {
            // if the old value is not nullptr, we need to remove the use from the use list
            remove_self();
        }
        if ((_value = value) != nullptr) {
            // if the new value is not nullptr, we need to insert the use to the use list
            _value->use_list().insert_front(this);
        }
    }
}

}// namespace luisa::compute::xir
