#pragma once
#include "string_builder.h"
#include "register.h"
namespace luisa::compute {
class Type;
}// namespace luisa::compute
namespace lc::spirv {
using namespace luisa::compute;

class CodegenLib {
    struct BufferType {
        Register srv_struct_type;
        Register srv_ptr_type;
        Register uav_struct_type;
        Register uav_ptr_type;
    };
    using LiteralVariantType = luisa::variant<
        double,
        int64_t,
        uint64_t>;
    struct LiteralVariantTypeHash {
        size_t operator()(LiteralVariantType const &h) const {
            return luisa::visit(
                [&]<typename T>(T const &t) {
                    return luisa::hash64(&t, sizeof(T), h.index());
                },
                h);
        }
    };
    struct LiteralVariantTypeCompare {
        int operator()(LiteralVariantType const &a, LiteralVariantType const &b) const {
            if (a.index() > b.index()) {
                return 1;
            } else if (a.index() < b.index()) {
                return -1;
            } else {
                return luisa::visit(
                    [&]<typename T>(T const &t) {
                        auto &&b_value = *(b.get_as<std::add_pointer_t<T>>());
                        if (t < b_value) return -1;
                        if (t > b_value) return 1;
                        return 0;
                    },
                    a);
            }
        }
    };
    vstd::StringBuilder _annotation_builder;
    vstd::StringBuilder _types_builder;
    vstd::StringBuilder _body_builder;
    vstd::HashMap<Type const *, Register> _types;
    vstd::HashMap<Type const *, Register> _runtime_arr_types;// usually used by buffer
    vstd::HashMap<Type const *, BufferType> _storage_types;
    vstd::HashMap<LiteralVariantType, Register, LiteralVariantTypeHash, LiteralVariantTypeCompare> _constant_values;
    Register _accel_id;
    Register _accel_ptr_id;
    Register _accel_inst_buffer_id;
    Register _accel_inst_ptr_id;
    Register _rw_accel_inst_buffer_id;
    Register _rw_accel_inst_ptr_id;
    Register _mark_runtime_arr(Type const *element);
    BufferType const& _mark_buffer_type(Type const *buffer, bool unordered_access);
    BufferType const& _mark_image_type(Type const *texture, bool unordered_access);
    Register _mark_constant_value(LiteralVariantType const& value);

public:
    CodegenLib();
    ~CodegenLib();
    Register mark_type(Type const *type, bool unordered_access = false);
};
}// namespace lc::spirv