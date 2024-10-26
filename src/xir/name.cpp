#include <luisa/core/logging.h>
#include <luisa/xir/name.h>

namespace luisa::compute::xir {

Name::Name(Pool *pool, luisa::string s) noexcept
    : PooledObject{pool}, _s{std::move(s)} {
    // validate the name
    LUISA_DEBUG_ASSERT(!_s.empty(), "Name cannot be empty.");
    constexpr auto is_value_name_head [[maybe_unused]] = [](char c) noexcept {
        return std::isalpha(c) || c == '_';
    };
    constexpr auto is_value_name_tail [[maybe_unused]] = [](char c) noexcept {
        return std::isalnum(c) || c == '_';
    };
    LUISA_DEBUG_ASSERT(is_value_name_head(_s.front()),
                       "Invalid name '{}'. Name must start "
                       "with a letter or underscore.",
                       _s);
    for (auto i = 1u; i < _s.size(); ++i) {
        LUISA_DEBUG_ASSERT(is_value_name_tail(_s[i]),
                           "Invalid name '{}'. Name must contain "
                           "only letters, digits or underscores.",
                           _s);
    }
}

}// namespace luisa::compute::xir
