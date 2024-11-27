#include "codegen_lib.h"
#include <luisa/ast/type.h>
#include <luisa/core/stl/format.h>

namespace lc::spirv {
CodegenLib::CodegenLib() {
    Register::reset_register();
}

Register CodegenLib::_mark_runtime_arr(Type const *element) {
    auto iter = _runtime_arr_types.try_emplace(element, vstd::lazy_eval([&] {
                                                   return Register::new_register();
                                               }));
    auto reg = iter.first.value();
    if (!iter.second) return reg;
    auto reg_str = reg.to_str();
    _annotation_builder << luisa::format("OpDecorate {} ArrayStride {}\n", reg_str, element->size());
    auto ele_type = mark_type(element);
    _types_builder << luisa::format("{} = OpTypeRuntimeArray {}\n", reg_str, ele_type.to_str());
    return reg;
}

auto CodegenLib::_mark_image_type(Type const *texture, bool unordered_access) -> BufferType const & {
    auto tex_type = _storage_types.emplace(texture).value();
    auto &struct_type = unordered_access ? tex_type.uav_struct_type : tex_type.srv_struct_type;
    auto &ptr_type = unordered_access ? tex_type.uav_ptr_type : tex_type.srv_ptr_type;
    if (struct_type != nullptr && ptr_type != nullptr) return tex_type;
    struct_type = Register::new_register();
    ptr_type = Register::new_register();
    auto struct_type_str = struct_type.to_str();
    auto ptr_type_str = ptr_type.to_str();
    luisa::string img_format;
    if (unordered_access) {
        switch (texture->element()->tag()) {
            case Type::Tag::INT32: {
                img_format = "Rgba32i";
            } break;
            case Type::Tag::UINT32: {
                img_format = "Rgba32iu";
            } break;
            case Type::Tag::FLOAT32: {
                img_format = "Rgba32f";
            } break;
            default: {
                LUISA_ERROR("Bad texture element type.");
            } break;
        }
    } else {
        img_format = "Unknown";
    }
    auto ele_type = mark_type(texture->element());
    _types_builder
        << luisa::format("{} = OpTypeImage {} {}D 2 0 0 {} {}\n",
                         struct_type_str,
                         ele_type.to_str(),
                         texture->dimension(),
                         unordered_access ? 1 : 2,
                         img_format)
        << luisa::format("{} = OpTypePointer UniformConstant {}\n",
                         ptr_type_str,
                         struct_type_str);
    return tex_type;
}

auto CodegenLib::_mark_buffer_type(Type const *buffer, bool unordered_access) -> BufferType const & {
    auto buffer_type = _storage_types.emplace(buffer).value();
    auto &struct_type = unordered_access ? buffer_type.uav_struct_type : buffer_type.srv_struct_type;
    auto &ptr_type = unordered_access ? buffer_type.uav_ptr_type : buffer_type.srv_ptr_type;
    if (struct_type != nullptr && ptr_type != nullptr) return buffer_type;
    struct_type = Register::new_register();
    ptr_type = Register::new_register();
    auto struct_type_str = struct_type.to_str();
    auto ptr_type_str = ptr_type.to_str();
    auto runtime_arr_reg_str = _mark_runtime_arr(buffer->element()).to_str();
    _types_builder << luisa::format("{} = OpTypeStruct {}\n", struct_type_str, runtime_arr_reg_str)
                   << luisa::format("{} = OpTypePointer Uniform {}\n", ptr_type_str, struct_type_str);
    _annotation_builder << luisa::format("OpMemberDecorate {} 0 Offset 0\n", struct_type_str);
    if (!unordered_access) {
        _annotation_builder << luisa::format("OpMemberDecorate {} 0 NonWritable\n", struct_type_str);
    }
    _annotation_builder << luisa::format("OpDecorate {} BufferBlock\n", struct_type_str);
    return buffer_type;
}

Register CodegenLib::_mark_constant_value(LiteralVariantType const &value) {
    auto iter = _constant_values.try_emplace(value, vstd::lazy_eval([] {
                                                 return Register::new_register();
                                             }));
    auto reg = iter.first.value();
    if (!iter.second) return reg;
    luisa::visit([&]<typename T>(T const &t) {
        auto ele_type = mark_type(Type::of<T>());
        _types_builder << luisa::format("{} = OpConstant {} {}\n", reg.to_str(), ele_type.to_str(), t);
    },
                 value);
    return reg;
}

Register CodegenLib::mark_type(Type const *type, bool unordered_access) {
    switch (type->tag()) {
        case Type::Tag::BUFFER: {
            auto &&t = _mark_buffer_type(type, unordered_access);
            return unordered_access ? t.uav_ptr_type : t.srv_ptr_type;
        }
        case Type::Tag::TEXTURE: {
            auto &&t = _mark_image_type(type, unordered_access);
            return unordered_access ? t.uav_ptr_type : t.srv_ptr_type;
        }
        case Type::Tag::BINDLESS_ARRAY: {
            // TODO: backend bindless
            LUISA_ERROR("Not implemented.");
        }
        case Type::Tag::ACCEL: {
            if (_accel_id == nullptr) {
                _accel_id = Register::new_register();
                _accel_ptr_id = Register::new_register();
                auto accel_id_id = _accel_id.to_str();
                _types_builder << luisa::format("{} = OpTypeAccelerationStructureKHR\n", accel_id_id)
                               << luisa::format("{} = OpTypePointer UniformConstant {}\n", _accel_ptr_id.to_str(), accel_id_id);
            }
            // TODO: backend accel instance buffer
            return _accel_id;
        }
    }
    auto iter =
        _types.try_emplace(type, vstd::lazy_eval([&] {
                               return Register::new_register();
                           }));
    auto reg = iter.first.value();
    if (!iter.second)
        return reg;
    auto reg_str = reg.to_str();
    if (type == nullptr) {
        _types_builder << luisa::format("{} = OpTypeVoid\n", reg_str);
        return reg;
    }
    switch (type->tag()) {
        case Type::Tag::BOOL:
            _types_builder << luisa::format("{} = OpTypeBool\n", reg_str);
            break;
        case Type::Tag::INT8:
            _types_builder << luisa::format("{} = OpTypeInt 8 1\n", reg_str);
            break;
        case Type::Tag::UINT8:
            _types_builder << luisa::format("{} = OpTypeInt 8 0\n", reg_str);
            break;
        case Type::Tag::INT16:
            _types_builder << luisa::format("{} = OpTypeInt 16, 1\n", reg_str);
            break;
        case Type::Tag::UINT16:
            _types_builder << luisa::format("{} = OpTypeInt 16 0\n", reg_str);
            break;
        case Type::Tag::INT32:
            _types_builder << luisa::format("{} = OpTypeInt 32 1\n", reg_str);
            break;
        case Type::Tag::UINT32:
            _types_builder << luisa::format("{} = OpTypeInt 32 0\n", reg_str);
            break;
        case Type::Tag::INT64:
            _types_builder << luisa::format("{} = OpTypeInt 64 1\n", reg_str);
            break;
        case Type::Tag::UINT64:
            _types_builder << luisa::format("{} = OpTypeInt 64 0\n", reg_str);
            break;
        case Type::Tag::FLOAT16:
            _types_builder << luisa::format("{} = OpTypeFloat 16\n", reg_str);
            break;
        case Type::Tag::FLOAT32:
            _types_builder << luisa::format("{} = OpTypeFloat 32\n", reg_str);
            break;
        case Type::Tag::FLOAT64:
            _types_builder << luisa::format("{} = OpTypeFloat 64\n", reg_str);
            break;
        case Type::Tag::VECTOR: {
            auto ele_type = mark_type(type->element());
            _types_builder << luisa::format("{} = OpTypeVector {} {}\n", reg_str, ele_type.to_str(), type->dimension());
        } break;
        case Type::Tag::MATRIX: {
            auto col_reg = mark_type(Type::vector(Type::of<float>(), type->dimension()));
            _types_builder << luisa::format("{} = OpTypeMatrix {} {}\n", reg_str, col_reg.to_str(), type->dimension());
        } break;
        case Type::Tag::ARRAY: {
            _annotation_builder << luisa::format("OpDecorate {} ArrayStride {}\n", reg_str, type->element()->size());
            auto ele_type = mark_type(type->element());
            _types_builder << luisa::format(
                "{} = OpTypeArray {} {}\n",
                reg_str,
                ele_type.to_str(),
                type->dimension());
        } break;
        case Type::Tag::STRUCTURE: {
            size_t count = 0;
            size_t offset = 0;
            for (auto &mem : type->members()) {
                offset = (offset + (mem->alignment() - 1)) & (~(mem->alignment() - 1));
                _annotation_builder << luisa::format("OpMemberDecorate {} {} Offset {}\n", reg_str, count, offset);
                offset += mem->size();
                count++;
            }
        } break;
    }
    return reg;
}
CodegenLib::~CodegenLib() {
}
}// namespace lc::spirv