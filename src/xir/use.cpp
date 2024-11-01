#include <luisa/core/logging.h>
#include <luisa/xir/value.h>
#include <luisa/xir/use.h>

namespace luisa::compute::xir {

Use::Use(User *user) noexcept : _user{user} {
    LUISA_DEBUG_ASSERT(user != nullptr, "User must not be null.");
}

void Use::reset_value() noexcept {
    if (_value) {
        // if the old value is not nullptr, we need to remove the use from the use list
        remove_self();
        _value = nullptr;
    }
}

void Use::set_value(Value *value, bool add_to_use_list) noexcept {
    if (_value != value) {// we don't need to do anything if the value is not changed
        reset_value();
        if ((_value = value) != nullptr && add_to_use_list) {
            // if the new value is not nullptr, we need to insert the use to the use list
            _value->use_list().insert_front(this);
        }
    }
}

}// namespace luisa::compute::xir
