#include <luisa/core/stl/hash.h>
#include <luisa/core/logging.h>
#include <luisa/ast/type_registry.h>
#include <luisa/xir/constant.h>

namespace luisa::compute::xir {

Constant::Constant(const Type *type, const void *data) noexcept
    : Super{type} {
    LUISA_ASSERT(type != nullptr, "Constant type must be specified.");
    if (!_is_small()) {
        _large = luisa::allocate_with_allocator<std::byte>(type->size());
    }
    set_data(data);
}

Constant::~Constant() noexcept {
    if (!_is_small()) {
        luisa::deallocate_with_allocator(static_cast<std::byte *>(_large));
    }
}

bool Constant::_is_small() const noexcept {
    return type()->size() <= sizeof(void *);
}

void Constant::_error_cannot_change_type() const noexcept {
    LUISA_ERROR_WITH_LOCATION("Constant type cannot be changed.");
}

namespace detail {

static void xir_constant_fill_data(const Type *t, const void *raw, void *data) noexcept {
    LUISA_DEBUG_ASSERT(t != nullptr && raw != nullptr && data != nullptr, "Invalid arguments.");
    if (t->is_bool()) {
        auto value = static_cast<uint8_t>(*static_cast<const bool *>(raw) ? 1u : 0u);
        memmove(data, &value, 1);
    } else if (t->is_scalar()) {
        memmove(data, raw, t->size());
    } else {
        switch (t->tag()) {
            case Type::Tag::VECTOR: {
                auto elem_type = t->element();
                auto dim = t->dimension();
                for (auto i = 0u; i < dim; i++) {
                    auto offset = i * elem_type->size();
                    auto raw_elem = static_cast<const std::byte *>(raw) + offset;
                    auto data_elem = static_cast<std::byte *>(data) + offset;
                    xir_constant_fill_data(elem_type, raw_elem, data_elem);
                }
                break;
            }
            case Type::Tag::MATRIX: {
                auto elem_type = t->element();
                auto dim = t->dimension();
                auto col_type = Type::vector(elem_type, dim);
                for (auto i = 0u; i < dim; i++) {
                    auto offset = i * col_type->size();
                    auto raw_col = static_cast<const std::byte *>(raw) + offset;
                    auto data_col = static_cast<std::byte *>(data) + offset;
                    xir_constant_fill_data(col_type, raw_col, data_col);
                }
                break;
            }
            case Type::Tag::ARRAY: {
                auto elem_type = t->element();
                auto dim = t->dimension();
                for (auto i = 0u; i < dim; i++) {
                    auto offset = i * elem_type->size();
                    auto raw_elem = static_cast<const std::byte *>(raw) + offset;
                    auto data_elem = static_cast<std::byte *>(data) + offset;
                    xir_constant_fill_data(elem_type, raw_elem, data_elem);
                }
                break;
            }
            case Type::Tag::STRUCTURE: {
                size_t offset = 0u;
                for (auto m : t->members()) {
                    offset = luisa::align(offset, m->alignment());
                    auto raw_member = static_cast<const std::byte *>(raw) + offset;
                    auto data_member = static_cast<std::byte *>(data) + offset;
                    xir_constant_fill_data(m, raw_member, data_member);
                    offset += m->size();
                }
                break;
            }
            default: LUISA_ERROR_WITH_LOCATION("Unsupported constant type.");
        }
    }
}

static void xir_constant_fill_one(const Type *t, void *data) noexcept {
    LUISA_DEBUG_ASSERT(t != nullptr && data != nullptr, "Invalid arguments.");
    if (t->is_bool()) {
        auto value = static_cast<uint8_t>(1u);
        memmove(data, &value, 1);
    } else if (t->is_scalar()) {
        auto do_copy = [data, t]<typename T>(T x) noexcept {
            LUISA_DEBUG_ASSERT(Type::of<T>() == t, "Type mismatch.");
            memmove(data, &x, sizeof(T));
        };
        switch (t->tag()) {
            case Type::Tag::INT8: do_copy(static_cast<int8_t>(1)); break;
            case Type::Tag::UINT8: do_copy(static_cast<uint8_t>(1)); break;
            case Type::Tag::INT16: do_copy(static_cast<int16_t>(1)); break;
            case Type::Tag::UINT16: do_copy(static_cast<uint16_t>(1)); break;
            case Type::Tag::INT32: do_copy(static_cast<int32_t>(1)); break;
            case Type::Tag::UINT32: do_copy(static_cast<uint32_t>(1)); break;
            case Type::Tag::INT64: do_copy(static_cast<int64_t>(1)); break;
            case Type::Tag::UINT64: do_copy(static_cast<uint64_t>(1)); break;
            case Type::Tag::FLOAT16: do_copy(static_cast<half>(1.0f)); break;
            case Type::Tag::FLOAT32: do_copy(static_cast<float>(1.0f)); break;
            case Type::Tag::FLOAT64: do_copy(static_cast<double>(1.0)); break;
            default: LUISA_ERROR_WITH_LOCATION("Unsupported scalar type.");
        }
    } else {
        switch (t->tag()) {
            case Type::Tag::VECTOR: {
                auto elem_type = t->element();
                auto dim = t->dimension();
                for (auto i = 0u; i < dim; i++) {
                    auto offset = i * elem_type->size();
                    auto data_elem = static_cast<std::byte *>(data) + offset;
                    xir_constant_fill_one(elem_type, data_elem);
                }
                break;
            }
            case Type::Tag::MATRIX: {
                auto elem_type = t->element();
                auto dim = t->dimension();
                auto col_type = Type::vector(elem_type, dim);
                for (auto i = 0u; i < dim; i++) {
                    auto offset = i * col_type->size();
                    auto data_col = static_cast<std::byte *>(data) + offset;
                    xir_constant_fill_one(col_type, data_col);
                }
                break;
            }
            case Type::Tag::ARRAY: {
                auto elem_type = t->element();
                auto dim = t->dimension();
                for (auto i = 0u; i < dim; i++) {
                    auto offset = i * elem_type->size();
                    auto data_elem = static_cast<std::byte *>(data) + offset;
                    xir_constant_fill_one(elem_type, data_elem);
                }
                break;
            }
            case Type::Tag::STRUCTURE: {
                size_t offset = 0u;
                for (auto m : t->members()) {
                    offset = luisa::align(offset, m->alignment());
                    auto data_member = static_cast<std::byte *>(data) + offset;
                    xir_constant_fill_one(m, data_member);
                    offset += m->size();
                }
                break;
            }
            default: LUISA_ERROR_WITH_LOCATION("Unsupported constant type.");
        }
    }
}

}// namespace detail

void Constant::set_data(const void *data) noexcept {
    memset(this->data(), 0, type()->size());
    if (data != nullptr) { detail::xir_constant_fill_data(type(), data, this->data()); }
    _update_hash();
}

void *Constant::data() noexcept {
    return _is_small() ? _small : _large;
}

const void *Constant::data() const noexcept {
    return const_cast<Constant *>(this)->data();
}

void Constant::set_zero() noexcept {
    set_data(nullptr);
}

void Constant::set_one() noexcept {
    memset(this->data(), 0, type()->size());
    detail::xir_constant_fill_one(type(), this->data());
    _update_hash();
}

void Constant::_check_reinterpret_cast_type_size(size_t size) const noexcept {
    LUISA_ASSERT(type()->size() == size, "Type size mismatch.");
}

void Constant::_update_hash() noexcept {
    auto hv = luisa::hash64(this->data(), type()->size(), luisa::hash64_default_seed);
    _hash = luisa::hash_combine({type()->hash(), hv});
}

}// namespace luisa::compute::xir
