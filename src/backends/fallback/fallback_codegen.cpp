#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/MC/TargetRegistry.h>

#include <luisa/core/logging.h>
#include <luisa/xir/module.h>
#include <luisa/xir/builder.h>
#include <luisa/xir/metadata/name.h>
#include <luisa/xir/metadata/location.h>

#include "fallback_codegen.h"

#include "luisa/core/stl/unordered_map.h"

namespace luisa::compute::fallback {

class FallbackCodegen {

private:
    struct LLVMStruct {
        llvm::StructType *type;
        luisa::vector<uint> member_indices;
    };

private:
    llvm::LLVMContext &_llvm_context;
    llvm::Module *_llvm_module = nullptr;
    luisa::unordered_map<const Type *, luisa::unique_ptr<LLVMStruct>> _llvm_struct_types;
    luisa::unordered_map<const xir::Constant *, llvm::Constant *> _llvm_constants;

private:
    void _reset() noexcept {
        // TODO
        _llvm_module = nullptr;
        _llvm_struct_types.clear();
        _llvm_constants.clear();
    }

private:
    [[nodiscard]] LLVMStruct *_translate_struct_type(const Type *t) noexcept {
        auto iter = _llvm_struct_types.try_emplace(t, nullptr).first;
        if (iter->second) { return iter->second.get(); }
        auto struct_type = (iter->second = luisa::make_unique<LLVMStruct>()).get();
        auto member_index = 0u;
        luisa::vector<::llvm::Type *> field_types;
        luisa::vector<uint> field_indices;
        size_t size = 0u;
        for (auto member : t->members()) {
            auto aligned_offset = luisa::align(size, member->alignment());
            if (aligned_offset > size) {
                auto byte_type = ::llvm::Type::getInt8Ty(_llvm_context);
                auto padding = ::llvm::ArrayType::get(byte_type, aligned_offset - size);
                field_types.emplace_back(padding);
                member_index++;
            }
            auto member_type = _translate_type(member);
            field_types.emplace_back(member_type);
            field_indices.emplace_back(member_index++);
            size = aligned_offset + member->size();
        }
        if (t->size() > size) {// last padding
            auto byte_type = ::llvm::Type::getInt8Ty(_llvm_context);
            auto padding = ::llvm::ArrayType::get(byte_type, t->size() - size);
            field_types.emplace_back(padding);
        }
        llvm::ArrayRef llvm_fields{field_types.data(), field_types.size()};
        struct_type->type = ::llvm::StructType::create(_llvm_context, llvm_fields);
        struct_type->member_indices = std::move(field_indices);
        return struct_type;
    }

    [[nodiscard]] llvm::Type *_translate_type(const Type *t) noexcept {
        if (t == nullptr) { return llvm::Type::getVoidTy(_llvm_context); }
        switch (t->tag()) {
            case Type::Tag::BOOL: return llvm::Type::getInt8Ty(_llvm_context);
            case Type::Tag::FLOAT16: return llvm::Type::getHalfTy(_llvm_context);
            case Type::Tag::FLOAT32: return llvm::Type::getFloatTy(_llvm_context);
            case Type::Tag::FLOAT64: return llvm::Type::getDoubleTy(_llvm_context);
            case Type::Tag::INT8: return llvm::Type::getInt8Ty(_llvm_context);
            case Type::Tag::INT16: return llvm::Type::getInt16Ty(_llvm_context);
            case Type::Tag::INT32: return llvm::Type::getInt32Ty(_llvm_context);
            case Type::Tag::INT64: return llvm::Type::getInt64Ty(_llvm_context);
            case Type::Tag::UINT8: return llvm::Type::getInt8Ty(_llvm_context);
            case Type::Tag::UINT16: return llvm::Type::getInt16Ty(_llvm_context);
            case Type::Tag::UINT32: return llvm::Type::getInt32Ty(_llvm_context);
            case Type::Tag::UINT64: return llvm::Type::getInt64Ty(_llvm_context);
            case Type::Tag::VECTOR: {
                auto elem_type = _translate_type(t->element());
                return llvm::VectorType::get(elem_type, t->dimension(), false);
            }
            case Type::Tag::MATRIX: {
                auto col_type = _translate_type(Type::vector(t->element(), t->dimension()));
                return llvm::ArrayType::get(col_type, t->dimension());
            }
            case Type::Tag::ARRAY: {
                auto elem_type = _translate_type(t->element());
                return llvm::ArrayType::get(elem_type, t->dimension());
            }
            case Type::Tag::STRUCTURE: return _translate_struct_type(t)->type;
            case Type::Tag::BUFFER: LUISA_NOT_IMPLEMENTED();
            case Type::Tag::TEXTURE: LUISA_NOT_IMPLEMENTED();
            case Type::Tag::BINDLESS_ARRAY: LUISA_NOT_IMPLEMENTED();
            case Type::Tag::ACCEL: LUISA_NOT_IMPLEMENTED();
            case Type::Tag::CUSTOM: LUISA_NOT_IMPLEMENTED();
        }
        LUISA_ERROR_WITH_LOCATION("Invalid type: {}.", t->description());
    }

    [[nodiscard]] llvm::Constant *_translate_literal(const Type *t, const void *data) {
        auto llvm_type = _translate_type(t);
        switch (t->tag()) {
            case Type::Tag::BOOL: return llvm::ConstantInt::get(llvm_type, *static_cast<const uint8_t *>(data));
            case Type::Tag::INT8: return llvm::ConstantInt::get(llvm_type, *static_cast<const int8_t *>(data));
            case Type::Tag::UINT8: return llvm::ConstantInt::get(llvm_type, *static_cast<const uint8_t *>(data));
            case Type::Tag::INT16: return llvm::ConstantInt::get(llvm_type, *static_cast<const int16_t *>(data));
            case Type::Tag::UINT16: return llvm::ConstantInt::get(llvm_type, *static_cast<const uint16_t *>(data));
            case Type::Tag::INT32: return llvm::ConstantInt::get(llvm_type, *static_cast<const int32_t *>(data));
            case Type::Tag::UINT32: return llvm::ConstantInt::get(llvm_type, *static_cast<const uint32_t *>(data));
            case Type::Tag::INT64: return llvm::ConstantInt::get(llvm_type, *static_cast<const int64_t *>(data));
            case Type::Tag::UINT64: return llvm::ConstantInt::get(llvm_type, *static_cast<const uint64_t *>(data));
            case Type::Tag::FLOAT16: return llvm::ConstantFP::get(llvm_type, *static_cast<const half *>(data));
            case Type::Tag::FLOAT32: return llvm::ConstantFP::get(llvm_type, *static_cast<const float *>(data));
            case Type::Tag::FLOAT64: return llvm::ConstantFP::get(llvm_type, *static_cast<const double *>(data));
            case Type::Tag::VECTOR: {
                auto elem_type = t->element();
                auto stride = elem_type->size();
                auto dim = t->dimension();
                luisa::fixed_vector<llvm::Constant *, 4u> elements;
                for (auto i = 0u; i < dim; i++) {
                    auto elem_data = static_cast<const std::byte *>(data) + i * stride;
                    elements.emplace_back(_translate_literal(elem_type, elem_data));
                }
                llvm::ArrayRef llvm_elements{elements.data(), elements.size()};
                return llvm::ConstantVector::get(llvm_elements);
            }
            case Type::Tag::MATRIX: {
                LUISA_ASSERT(llvm_type->isArrayTy(), "Matrix type should be an array type.");
                auto col_type = Type::vector(t->element(), t->dimension());
                auto col_stride = col_type->size();
                auto dim = t->dimension();
                luisa::fixed_vector<llvm::Constant *, 4u> elements;
                for (auto i = 0u; i < dim; i++) {
                    auto col_data = static_cast<const std::byte *>(data) + i * col_stride;
                    elements.emplace_back(_translate_literal(col_type, col_data));
                }
                llvm::ArrayRef llvm_elements{elements.data(), elements.size()};
                return llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(llvm_type), llvm_elements);
            }
            case Type::Tag::ARRAY: {
                LUISA_ASSERT(llvm_type->isArrayTy(), "Array type should be an array type.");
                auto elem_type = t->element();
                auto stride = elem_type->size();
                auto dim = t->dimension();
                luisa::vector<llvm::Constant *> elements;
                elements.reserve(dim);
                for (auto i = 0u; i < dim; i++) {
                    auto elem_data = static_cast<const std::byte *>(data) + i * stride;
                    elements.emplace_back(_translate_literal(elem_type, elem_data));
                }
                llvm::ArrayRef llvm_elements{elements.data(), elements.size()};
                return llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(llvm_type), llvm_elements);
            }
            case Type::Tag::STRUCTURE: {
                auto struct_type = _translate_struct_type(t);
                LUISA_ASSERT(llvm_type == struct_type->type, "Type mismatch.");
                luisa::vector<llvm::Constant *> elements;
                // TODO: WORK IN PROGRESS
            }
            default: break;
        }
        LUISA_ERROR_WITH_LOCATION("Invalid type: {}.", t->description());
    }

    [[nodiscard]] llvm::Constant *_translate_constant(const xir::Constant *c) {
        if (auto iter = _llvm_constants.find(c); iter != _llvm_constants.end()) {
            return iter->second;
        }
        auto type = c->type();
        auto llvm_value = _translate_literal(type, c->data());
        // promote non-basic constants to globals
        if (!type->is_basic()) {
            auto name = luisa::format("const{}", _llvm_constants.size());
            auto g = llvm::dyn_cast<llvm::GlobalVariable>(
                _llvm_module->getOrInsertGlobal(llvm::StringRef{name}, llvm_value->getType()));
            g->setConstant(true);
            g->setLinkage(llvm::GlobalValue::LinkageTypes::InternalLinkage);
            g->setInitializer(llvm_value);
            g->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
            llvm_value = g;
        }
        // cache
        _llvm_constants.emplace(c, llvm_value);
        return llvm_value;
    }

    void _translate_module(const xir::Module *module) noexcept {
    }

public:
    explicit FallbackCodegen(llvm::LLVMContext &ctx) noexcept
        : _llvm_context{ctx} {}

    [[nodiscard]] auto emit(const xir::Module *module) noexcept {
        auto name_md = module->find_metadata<xir::NameMD>();
        auto location_md = module->find_metadata<xir::LocationMD>();
        auto module_name = name_md ? name_md->name() : "module";
        auto module_location = location_md ? location_md->file().string() : "unknown";
        auto llvm_module = std::make_unique<llvm::Module>(llvm::StringRef{module_name}, _llvm_context);
        llvm_module->setSourceFileName(location_md ? location_md->file().string() : "unknown");
        _llvm_module = llvm_module.get();
        _translate_module(module);
        _reset();
        return llvm_module;
    }
};

std::unique_ptr<llvm::Module>
luisa_fallback_backend_codegen(llvm::LLVMContext &llvm_ctx, const xir::Module *module) noexcept {
    FallbackCodegen codegen{llvm_ctx};
    return codegen.emit(module);
}

}// namespace luisa::compute::fallback
