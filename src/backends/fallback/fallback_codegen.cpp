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

#include <luisa/core/stl/unordered_map.h>
#include <luisa/core/logging.h>
#include <luisa/xir/module.h>
#include <luisa/xir/builder.h>
#include <luisa/xir/metadata/name.h>
#include <luisa/xir/metadata/location.h>

#include "fallback_codegen.h"

namespace luisa::compute::fallback {

class FallbackCodegen {

private:
    struct LLVMStruct {
        llvm::StructType *type = nullptr;
        luisa::vector<uint> padded_field_indices;
    };

    using IRBuilder = llvm::IRBuilder<>;

    struct CurrentFunction {
        llvm::Function *func = nullptr;
        luisa::unordered_map<const xir::Value *, llvm::Value *> value_map;
        luisa::unordered_set<const llvm::BasicBlock *> translated_basic_blocks;

        // builtin variables
#define LUISA_FALLBACK_BACKEND_DECL_BUILTIN_VARIABLE(NAME, INDEX) \
    static constexpr size_t builtin_variable_index_##NAME = INDEX;
        LUISA_FALLBACK_BACKEND_DECL_BUILTIN_VARIABLE(thread_id, 0)
        LUISA_FALLBACK_BACKEND_DECL_BUILTIN_VARIABLE(block_id, 1)
        LUISA_FALLBACK_BACKEND_DECL_BUILTIN_VARIABLE(dispatch_id, 2)
        LUISA_FALLBACK_BACKEND_DECL_BUILTIN_VARIABLE(block_size, 3)
        LUISA_FALLBACK_BACKEND_DECL_BUILTIN_VARIABLE(dispatch_size, 4)
#undef LUISA_FALLBACK_BACKEND_DECL_BUILTIN_VARIABLE
        static constexpr size_t builtin_variable_count = 5;
        llvm::Value *builtin_variables[builtin_variable_count] = {};
    };

private:
    llvm::LLVMContext &_llvm_context;
    llvm::Module *_llvm_module = nullptr;
    luisa::unordered_map<const Type *, luisa::unique_ptr<LLVMStruct>> _llvm_struct_types;
    luisa::unordered_map<const xir::Constant *, llvm::Constant *> _llvm_constants;
    luisa::unordered_map<const xir::Function *, llvm::Function *> _llvm_functions;

private:
    void _reset() noexcept {
        _llvm_module = nullptr;
        _llvm_struct_types.clear();
        _llvm_constants.clear();
        _llvm_functions.clear();
    }

private:
    [[nodiscard]] static llvm::StringRef _get_name_from_metadata(auto something, llvm::StringRef fallback = {}) noexcept {
        auto name_md = something->template find_metadata<xir::NameMD>();
        return name_md ? llvm::StringRef{name_md->name()} : fallback;
    }

    [[nodiscard]] LLVMStruct *_translate_struct_type(const Type *t) noexcept {
        auto iter = _llvm_struct_types.try_emplace(t, nullptr).first;
        if (iter->second) { return iter->second.get(); }
        auto struct_type = (iter->second = luisa::make_unique<LLVMStruct>()).get();
        auto member_index = 0u;
        llvm::SmallVector<::llvm::Type *> field_types;
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
            auto member_type = _translate_type(member, false);
            field_types.emplace_back(member_type);
            field_indices.emplace_back(member_index++);
            size = aligned_offset + member->size();
        }
        if (t->size() > size) {// last padding
            auto byte_type = ::llvm::Type::getInt8Ty(_llvm_context);
            auto padding = ::llvm::ArrayType::get(byte_type, t->size() - size);
            field_types.emplace_back(padding);
        }
        struct_type->type = ::llvm::StructType::create(_llvm_context, field_types);
        struct_type->padded_field_indices = std::move(field_indices);
        return struct_type;
    }

    [[nodiscard]] llvm::Type *_translate_type(const Type *t, bool register_use) noexcept {
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
                // note: we have to pad 3-element vectors to be 4-element
                auto elem_type = _translate_type(t->element(), false);
                auto dim = t->dimension();
                LUISA_ASSERT(dim == 2 || dim == 3 || dim == 4, "Invalid vector dimension.");
                if (register_use) { return llvm::VectorType::get(elem_type, dim, false); }
                return llvm::ArrayType::get(elem_type, dim == 3 ? 4 : dim);
            }
            case Type::Tag::MATRIX: {
                auto col_type = _translate_type(Type::vector(t->element(), t->dimension()), false);
                return llvm::ArrayType::get(col_type, t->dimension());
            }
            case Type::Tag::ARRAY: {
                auto elem_type = _translate_type(t->element(), false);
                return llvm::ArrayType::get(elem_type, t->dimension());
            }
            case Type::Tag::STRUCTURE: return _translate_struct_type(t)->type;
            case Type::Tag::BUFFER: return llvm::PointerType::get(_llvm_context, 0);
            case Type::Tag::TEXTURE: LUISA_NOT_IMPLEMENTED();
            case Type::Tag::BINDLESS_ARRAY: LUISA_NOT_IMPLEMENTED();
            case Type::Tag::ACCEL: LUISA_NOT_IMPLEMENTED();
            case Type::Tag::CUSTOM: LUISA_NOT_IMPLEMENTED();
        }
        LUISA_ERROR_WITH_LOCATION("Invalid type: {}.", t->description());
    }

    [[nodiscard]] llvm::Constant *_translate_literal(const Type *t, const void *data, bool register_use) noexcept {
        auto llvm_type = _translate_type(t, register_use);
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
                llvm::SmallVector<llvm::Constant *, 4u> elements;
                for (auto i = 0u; i < dim; i++) {
                    auto elem_data = static_cast<const std::byte *>(data) + i * stride;
                    elements.emplace_back(_translate_literal(elem_type, elem_data, false));
                }
                // for register use, we create an immediate vector
                if (register_use) {
                    return llvm::ConstantVector::get(elements);
                }
                // for memory use we have to pad 3-element vectors to 4-element
                if (dim == 3) {
                    auto llvm_elem_type = _translate_type(t->element(), false);
                    auto llvm_padding = llvm::Constant::getNullValue(llvm_elem_type);
                    elements.emplace_back(llvm_padding);
                }
                return llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(llvm_type), elements);
            }
            case Type::Tag::MATRIX: {
                LUISA_ASSERT(llvm_type->isArrayTy(), "Matrix type should be an array type.");
                auto dim = t->dimension();
                auto col_type = Type::vector(t->element(), dim);
                auto col_stride = col_type->size();
                llvm::SmallVector<llvm::Constant *, 4u> elements;
                for (auto i = 0u; i < dim; i++) {
                    auto col_data = static_cast<const std::byte *>(data) + i * col_stride;
                    elements.emplace_back(_translate_literal(col_type, col_data, false));
                }
                return llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(llvm_type), elements);
            }
            case Type::Tag::ARRAY: {
                LUISA_ASSERT(llvm_type->isArrayTy(), "Array type should be an array type.");
                auto elem_type = t->element();
                auto stride = elem_type->size();
                auto dim = t->dimension();
                llvm::SmallVector<llvm::Constant *> elements;
                elements.reserve(dim);
                for (auto i = 0u; i < dim; i++) {
                    auto elem_data = static_cast<const std::byte *>(data) + i * stride;
                    elements.emplace_back(_translate_literal(elem_type, elem_data, false));
                }
                return llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(llvm_type), elements);
            }
            case Type::Tag::STRUCTURE: {
                auto struct_type = _translate_struct_type(t);
                LUISA_ASSERT(llvm_type == struct_type->type, "Type mismatch.");
                auto padded_field_types = struct_type->type->elements();
                llvm::SmallVector<llvm::Constant *, 16u> fields;
                fields.resize(padded_field_types.size(), nullptr);
                LUISA_ASSERT(t->members().size() == struct_type->padded_field_indices.size(),
                             "Member count mismatch.");
                // fill in data fields
                size_t data_offset = 0u;
                for (auto i = 0u; i < t->members().size(); i++) {
                    auto field_type = t->members()[i];
                    data_offset = luisa::align(data_offset, field_type->alignment());
                    auto field_data = static_cast<const std::byte *>(data) + data_offset;
                    data_offset += field_type->size();
                    auto padded_index = struct_type->padded_field_indices[i];
                    fields[padded_index] = _translate_literal(field_type, field_data, false);
                }
                // fill in padding fields
                for (auto i = 0u; i < fields.size(); i++) {
                    if (fields[i] == nullptr) {
                        auto field_type = padded_field_types[i];
                        LUISA_ASSERT(field_type->isArrayTy(), "Padding field should be an array type.");
                        fields[i] = llvm::ConstantAggregateZero::get(field_type);
                    }
                }
                return llvm::ConstantStruct::get(struct_type->type, fields);
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
        // we have to create a global variable for non-basic constants
        auto llvm_value = _translate_literal(type, c->data(), type->is_basic());
        // promote non-basic constants to globals
        if (!type->is_basic()) {
            auto name = luisa::format("const{}", _llvm_constants.size());
            auto g = llvm::dyn_cast<llvm::GlobalVariable>(
                _llvm_module->getOrInsertGlobal(llvm::StringRef{name}, llvm_value->getType()));
            g->setConstant(true);
            g->setLinkage(llvm::GlobalValue::LinkageTypes::PrivateLinkage);
            g->setInitializer(llvm_value);
            g->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
            llvm_value = g;
        }
        // cache
        _llvm_constants.emplace(c, llvm_value);
        return llvm_value;
    }

    void _translate_module(const xir::Module *module) noexcept {
        for (auto &f : module->functions()) {
            static_cast<void>(_translate_function(&f));
        }
    }

    [[nodiscard]] llvm::Value *_lookup_value(CurrentFunction &current, IRBuilder &b, const xir::Value *v, bool load_global = true) noexcept {
        LUISA_ASSERT(v != nullptr, "Value is null.");
        switch (v->derived_value_tag()) {
            case xir::DerivedValueTag::FUNCTION: {
                return _translate_function(static_cast<const xir::Function *>(v));
            }
            case xir::DerivedValueTag::BASIC_BLOCK: {
                return _find_or_create_basic_block(current, static_cast<const xir::BasicBlock *>(v));
            }
            case xir::DerivedValueTag::CONSTANT: {
                auto c = _translate_constant(static_cast<const xir::Constant *>(v));
                if (load_global && c->getType()->isPointerTy()) {
                    auto llvm_type = _translate_type(v->type(), true);
                    auto alignment = v->type()->alignment();
                    return b.CreateAlignedLoad(llvm_type, c, llvm::MaybeAlign{alignment});
                }
                return c;
            }
            case xir::DerivedValueTag::INSTRUCTION: [[fallthrough]];
            case xir::DerivedValueTag::ARGUMENT: {
                auto iter = current.value_map.find(v);
                LUISA_ASSERT(iter != current.value_map.end(), "Value not found.");
                return iter->second;
            }
        }
        LUISA_ERROR_WITH_LOCATION("Invalid value.");
    }

    [[nodiscard]] llvm::Constant *_translate_string_or_null(IRBuilder &b, luisa::string_view s) noexcept {
        if (s.empty()) {
            auto ptr_type = llvm::PointerType::get(_llvm_context, 0);
            return llvm::ConstantPointerNull::get(ptr_type);
        }
        return b.CreateGlobalStringPtr(s);
    }

    [[nodiscard]] llvm::Value *_translate_gep(CurrentFunction &current, IRBuilder &b,
                                              const Type *expected_elem_type,
                                              const Type *ptr_type, llvm::Value *llvm_ptr,
                                              luisa::span<const xir::Use *const> indices) noexcept {
        LUISA_ASSERT(llvm_ptr->getType()->isPointerTy(), "Invalid pointer type.");
        while (!indices.empty()) {
            // get the index
            auto index = indices.front()->value();
            indices = indices.subspan(1u);
            // structures need special handling
            if (ptr_type->is_structure()) {
                LUISA_ASSERT(index->derived_value_tag() == xir::DerivedValueTag::CONSTANT,
                             "Structure index should be a constant.");
                auto static_index = [c = static_cast<const xir::Constant *>(index)]() noexcept -> uint64_t {
                    auto t = c->type();
                    switch (t->tag()) {
                        case Type::Tag::INT8: return *static_cast<const int8_t *>(c->data());
                        case Type::Tag::UINT8: return *static_cast<const uint8_t *>(c->data());
                        case Type::Tag::INT16: return *static_cast<const int16_t *>(c->data());
                        case Type::Tag::UINT16: return *static_cast<const uint16_t *>(c->data());
                        case Type::Tag::INT32: return *static_cast<const int32_t *>(c->data());
                        case Type::Tag::UINT32: return *static_cast<const uint32_t *>(c->data());
                        case Type::Tag::INT64: return *static_cast<const int64_t *>(c->data());
                        case Type::Tag::UINT64: return *static_cast<const uint64_t *>(c->data());
                        default: break;
                    }
                    LUISA_ERROR_WITH_LOCATION("Invalid index type: {}.", t->description());
                }();
                // remap the index for padded fields
                LUISA_ASSERT(static_index < ptr_type->members().size(), "Invalid structure index.");
                auto llvm_struct_type = _translate_struct_type(ptr_type);
                auto padded_index = llvm_struct_type->padded_field_indices[static_index];
                // index into the struct and update member type
                llvm_ptr = b.CreateStructGEP(llvm_struct_type->type, llvm_ptr, padded_index);
                ptr_type = ptr_type->members()[static_index];
            } else {
                LUISA_ASSERT(ptr_type->is_array() || ptr_type->is_vector() || ptr_type->is_matrix(), "Invalid pointer type.");
                auto llvm_index = _lookup_value(current, b, index);
                auto llvm_ptr_type = _translate_type(ptr_type, false);
                auto llvm_zero = b.getInt64(0);
                llvm_ptr = b.CreateInBoundsGEP(llvm_ptr_type, llvm_ptr, {llvm_zero, llvm_index});
                ptr_type = ptr_type->is_matrix() ?
                               Type::vector(ptr_type->element(), ptr_type->dimension()) :
                               ptr_type->element();
            }
        }
        LUISA_ASSERT(ptr_type == expected_elem_type, "Type mismatch.");
        return llvm_ptr;
    }

    [[nodiscard]] llvm::Value *_translate_extract(CurrentFunction &current, IRBuilder &b, const xir::IntrinsicInst *inst) noexcept {
        auto base = inst->operand(0u);
        auto indices = inst->operand_uses().subspan(1u);
        auto llvm_base = _lookup_value(current, b, base, false);
        // if we have an immediate vector, we can directly extract elements
        if (base->type()->is_vector() && !llvm_base->getType()->isPointerTy()) {
            LUISA_ASSERT(indices.size() == 1u, "Immediate vector should have only one index.");
            auto llvm_index = _lookup_value(current, b, indices.front()->value());
            return b.CreateExtractElement(llvm_base, llvm_index);
        }
        // otherwise we have to use GEP
        if (!llvm_base->getType()->isPointerTy()) {// create a temporary alloca if base is not a pointer
            auto llvm_base_type = _translate_type(base->type(), false);
            auto llvm_temp = b.CreateAlloca(llvm_base_type);
            auto alignment = base->type()->alignment();
            if (llvm_temp->getAlign() < alignment) {
                llvm_temp->setAlignment(llvm::Align{alignment});
            }
            b.CreateAlignedStore(llvm_base, llvm_temp, llvm::MaybeAlign{alignment});
            llvm_base = llvm_temp;
        }
        // GEP and load the element
        auto llvm_gep = _translate_gep(current, b, inst->type(), base->type(), llvm_base, indices);
        auto llvm_type = _translate_type(inst->type(), true);
        return b.CreateAlignedLoad(llvm_type, llvm_gep, llvm::MaybeAlign{inst->type()->alignment()});
    }

    [[nodiscard]] llvm::Value *_translate_insert(CurrentFunction &current, IRBuilder &b, const xir::IntrinsicInst *inst) noexcept {
        auto base = inst->operand(0u);
        auto value = inst->operand(1u);
        auto indices = inst->operand_uses().subspan(2u);
        auto llvm_base = _lookup_value(current, b, base, false);
        auto llvm_value = _lookup_value(current, b, value, false);
        // if we have an immediate vector, we can directly insert elements
        if (base->type()->is_vector() && !llvm_base->getType()->isPointerTy()) {
            LUISA_ASSERT(indices.size() == 1u, "Immediate vector should have only one index.");
            auto llvm_index = _lookup_value(current, b, indices.front()->value());
            return b.CreateInsertElement(llvm_base, llvm_value, llvm_index);
        }
        // otherwise we have to make a copy of the base, store the element, and load the modified value
        auto llvm_base_type = _translate_type(base->type(), false);
        auto llvm_temp = b.CreateAlloca(llvm_base_type);
        auto alignment = base->type()->alignment();
        if (llvm_temp->getAlign() < alignment) {
            llvm_temp->setAlignment(llvm::Align{alignment});
        }
        // load the base if it is a pointer
        if (llvm_base->getType()->isPointerTy()) {
            llvm_base = b.CreateAlignedLoad(llvm_base_type, llvm_base, llvm::MaybeAlign{alignment});
        }
        // store the base
        b.CreateAlignedStore(llvm_base, llvm_temp, llvm::MaybeAlign{alignment});
        // GEP and store the element
        auto llvm_gep = _translate_gep(current, b, value->type(), base->type(), llvm_temp, indices);
        b.CreateAlignedStore(llvm_value, llvm_gep, llvm::MaybeAlign{value->type()->alignment()});
        // load the modified value
        return b.CreateAlignedLoad(llvm_base_type, llvm_temp, llvm::MaybeAlign{alignment});
    }

    [[nodiscard]] llvm::Value *_translate_unary_plus(CurrentFunction &current, IRBuilder &b, const xir::Value *operand) noexcept {
        return _lookup_value(current, b, operand);
    }

    [[nodiscard]] llvm::Value *_translate_unary_minus(CurrentFunction &current, IRBuilder &b, const xir::Value *operand) noexcept {
        auto llvm_operand = _lookup_value(current, b, operand);
        auto operand_type = operand->type();
        LUISA_ASSERT(operand_type != nullptr, "Operand type is null.");
        auto operand_elem_type = operand_type->is_vector() ? operand_type->element() : operand_type;
        switch (operand_elem_type->tag()) {
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::INT64: return b.CreateNSWNeg(llvm_operand);
            case Type::Tag::UINT8: [[fallthrough]];
            case Type::Tag::UINT16: [[fallthrough]];
            case Type::Tag::UINT32: [[fallthrough]];
            case Type::Tag::UINT64: return b.CreateNeg(llvm_operand);
            case Type::Tag::FLOAT16: [[fallthrough]];
            case Type::Tag::FLOAT32: [[fallthrough]];
            case Type::Tag::FLOAT64: return b.CreateFNeg(llvm_operand);
            default: break;
        }
        LUISA_ERROR_WITH_LOCATION("Invalid unary minus operand type: {}.", operand_type->description());
    }

    [[nodiscard]] static llvm::Value *_cmp_ne_zero(IRBuilder &b, llvm::Value *value) noexcept {
        auto zero = llvm::Constant::getNullValue(value->getType());
        return value->getType()->isFPOrFPVectorTy() ?
                   b.CreateFCmpUNE(value, zero) :
                   b.CreateICmpNE(value, zero);
    }

    [[nodiscard]] static llvm::Value *_cmp_eq_zero(IRBuilder &b, llvm::Value *value) noexcept {
        auto zero = llvm::Constant::getNullValue(value->getType());
        return value->getType()->isFPOrFPVectorTy() ?
                   b.CreateFCmpUEQ(value, zero) :
                   b.CreateICmpEQ(value, zero);
    }

    [[nodiscard]] static llvm::Value *_zext_i1_to_i8(IRBuilder &b, llvm::Value *value) noexcept {
        LUISA_DEBUG_ASSERT(value->getType()->isIntOrIntVectorTy(1), "Invalid value type.");
        auto i8_type = llvm::cast<llvm::Type>(llvm::Type::getInt8Ty(b.getContext()));
        if (value->getType()->isVectorTy()) {
            auto vec_type = llvm::cast<llvm::VectorType>(value->getType());
            i8_type = llvm::VectorType::get(i8_type, vec_type->getElementCount());
        }
        return b.CreateZExt(value, i8_type);
    }

    [[nodiscard]] llvm::Value *_translate_unary_logic_not(CurrentFunction &current, IRBuilder &b, const xir::Value *operand) noexcept {
        auto llvm_operand = _lookup_value(current, b, operand);
        auto operand_type = operand->type();
        LUISA_ASSERT(operand_type != nullptr, "Operand type is null.");
        LUISA_ASSERT(operand_type->is_scalar() || operand_type->is_vector(), "Invalid operand type.");
        auto llvm_cmp = _cmp_eq_zero(b, llvm_operand);
        return _zext_i1_to_i8(b, llvm_cmp);
    }

    [[nodiscard]] llvm::Value *_translate_unary_bit_not(CurrentFunction &current, IRBuilder &b, const xir::Value *operand) noexcept {
        auto llvm_operand = _lookup_value(current, b, operand);
        LUISA_ASSERT(llvm_operand->getType()->isIntOrIntVectorTy() &&
                         !llvm_operand->getType()->isIntOrIntVectorTy(1),
                     "Invalid operand type.");
        return b.CreateNot(llvm_operand);
    }

    [[nodiscard]] llvm::Value *_translate_binary_add(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        LUISA_ASSERT(lhs->type() == rhs->type(), "Type mismatch.");
        auto llvm_lhs = _lookup_value(current, b, lhs);
        auto llvm_rhs = _lookup_value(current, b, rhs);
        auto elem_type = lhs->type()->is_vector() ? lhs->type()->element() : lhs->type();
        switch (elem_type->tag()) {
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::INT64: return b.CreateNSWAdd(llvm_lhs, llvm_rhs);
            case Type::Tag::UINT8: [[fallthrough]];
            case Type::Tag::UINT16: [[fallthrough]];
            case Type::Tag::UINT32: [[fallthrough]];
            case Type::Tag::UINT64: return b.CreateAdd(llvm_lhs, llvm_rhs);
            case Type::Tag::FLOAT16: [[fallthrough]];
            case Type::Tag::FLOAT32: [[fallthrough]];
            case Type::Tag::FLOAT64: return b.CreateFAdd(llvm_lhs, llvm_rhs);
            default: break;
        }
        LUISA_ERROR_WITH_LOCATION("Invalid binary add operand type: {}.", elem_type->description());
    }

    //swfly tries to write more binary operations
    [[nodiscard]] llvm::Value *_translate_binary_sub(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        LUISA_ASSERT(lhs->type() == rhs->type(), "Type mismatch.");
        auto llvm_lhs = _lookup_value(current, b, lhs);
        auto llvm_rhs = _lookup_value(current, b, rhs);
        auto elem_type = lhs->type()->is_vector() ? lhs->type()->element() : lhs->type();
        switch (elem_type->tag()) {
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::INT64: return b.CreateNSWSub(llvm_lhs, llvm_rhs);
            case Type::Tag::UINT8: [[fallthrough]];
            case Type::Tag::UINT16: [[fallthrough]];
            case Type::Tag::UINT32: [[fallthrough]];
            case Type::Tag::UINT64: return b.CreateSub(llvm_lhs, llvm_rhs);
            case Type::Tag::FLOAT16: [[fallthrough]];
            case Type::Tag::FLOAT32: [[fallthrough]];
            case Type::Tag::FLOAT64: return b.CreateFSub(llvm_lhs, llvm_rhs);
            default: break;
        }
        LUISA_ERROR_WITH_LOCATION("Invalid binary sub operand type: {}.", elem_type->description());
    }

    [[nodiscard]] llvm::Value *_translate_binary_mul(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        LUISA_ASSERT(lhs->type() == rhs->type(), "Type mismatch.");
        auto llvm_lhs = _lookup_value(current, b, lhs);
        auto llvm_rhs = _lookup_value(current, b, rhs);
        auto elem_type = lhs->type()->is_vector() ? lhs->type()->element() : lhs->type();
        switch (elem_type->tag()) {
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::INT64: return b.CreateNSWMul(llvm_lhs, llvm_rhs);
            case Type::Tag::UINT8: [[fallthrough]];
            case Type::Tag::UINT16: [[fallthrough]];
            case Type::Tag::UINT32: [[fallthrough]];
            case Type::Tag::UINT64: return b.CreateMul(llvm_lhs, llvm_rhs);
            case Type::Tag::FLOAT16: [[fallthrough]];
            case Type::Tag::FLOAT32: [[fallthrough]];
            case Type::Tag::FLOAT64: return b.CreateFMul(llvm_lhs, llvm_rhs);
            default: break;
        }
        LUISA_ERROR_WITH_LOCATION("Invalid binary mul operand type: {}.", elem_type->description());
    }

    [[nodiscard]] llvm::Value *_translate_binary_div(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        LUISA_ASSERT(lhs->type() == rhs->type(), "Type mismatch.");
        auto llvm_lhs = _lookup_value(current, b, lhs);
        auto llvm_rhs = _lookup_value(current, b, rhs);
        auto elem_type = lhs->type()->is_vector() ? lhs->type()->element() : lhs->type();
        switch (elem_type->tag()) {
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::INT64: return b.CreateSDiv(llvm_lhs, llvm_rhs);
            case Type::Tag::UINT8: [[fallthrough]];
            case Type::Tag::UINT16: [[fallthrough]];
            case Type::Tag::UINT32: [[fallthrough]];
            case Type::Tag::UINT64: return b.CreateUDiv(llvm_lhs, llvm_rhs);
            case Type::Tag::FLOAT16: [[fallthrough]];
            case Type::Tag::FLOAT32: [[fallthrough]];
            case Type::Tag::FLOAT64: return b.CreateFDiv(llvm_lhs, llvm_rhs);
            default: break;
        }
        LUISA_ERROR_WITH_LOCATION("Invalid binary add operand type: {}.", elem_type->description());
    }

    [[nodiscard]] llvm::Value *_translate_binary_mod(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        LUISA_ASSERT(lhs->type() == rhs->type(), "Type mismatch.");
        auto llvm_lhs = _lookup_value(current, b, lhs);
        auto llvm_rhs = _lookup_value(current, b, rhs);
        auto elem_type = lhs->type()->is_vector() ? lhs->type()->element() : lhs->type();
        switch (elem_type->tag()) {
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::INT64: return b.CreateSRem(llvm_lhs, llvm_rhs);// Signed integer remainder
            case Type::Tag::UINT8: [[fallthrough]];
            case Type::Tag::UINT16: [[fallthrough]];
            case Type::Tag::UINT32: [[fallthrough]];
            case Type::Tag::UINT64: return b.CreateURem(llvm_lhs, llvm_rhs);// Unsigned integer remainder
            default: break;
        }
        LUISA_ERROR_WITH_LOCATION("Invalid binary mod operand type: {}.", elem_type->description());
    }

    [[nodiscard]] llvm::Value *_translate_binary_logic_and(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        // Lookup LLVM values for operands
        auto llvm_lhs = _lookup_value(current, b, lhs);
        auto llvm_rhs = _lookup_value(current, b, rhs);
        auto lhs_type = lhs->type();
        auto rhs_type = rhs->type();
        // Type and null checks
        LUISA_ASSERT(lhs_type != nullptr && rhs_type != nullptr, "Operand type is null.");
        LUISA_ASSERT(lhs_type == rhs_type, "Type mismatch for logic and.");
        LUISA_ASSERT(lhs_type->is_scalar() || lhs_type->is_vector(), "Invalid operand type.");
        LUISA_ASSERT(rhs_type->is_scalar() || rhs_type->is_vector(), "Invalid operand type.");
        // Convert operands to boolean values (non-zero becomes true, zero becomes false)
        auto llvm_lhs_bool = _cmp_ne_zero(b, llvm_lhs);
        auto llvm_rhs_bool = _cmp_ne_zero(b, llvm_rhs);
        // Perform logical AND (a && b)
        auto llvm_and_result = b.CreateAnd(llvm_lhs_bool, llvm_rhs_bool);
        // Convert result to i8 for consistency with your implementation needs
        return _zext_i1_to_i8(b, llvm_and_result);
    }

    [[nodiscard]] llvm::Value *_translate_binary_logic_or(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        // Lookup LLVM values for operands
        auto llvm_lhs = _lookup_value(current, b, lhs);
        auto llvm_rhs = _lookup_value(current, b, rhs);
        auto lhs_type = lhs->type();
        auto rhs_type = rhs->type();
        // Type and null checks
        LUISA_ASSERT(lhs_type != nullptr && rhs_type != nullptr, "Operand type is null.");
        LUISA_ASSERT(lhs_type == rhs_type, "Type mismatch for logic and.");
        LUISA_ASSERT(lhs_type->is_scalar() || lhs_type->is_vector(), "Invalid operand type.");
        LUISA_ASSERT(rhs_type->is_scalar() || rhs_type->is_vector(), "Invalid operand type.");
        // Convert operands to boolean values (non-zero becomes true, zero becomes false)
        auto llvm_lhs_bool = _cmp_ne_zero(b, llvm_lhs);
        auto llvm_rhs_bool = _cmp_ne_zero(b, llvm_rhs);
        // Perform logical OR (a && b)
        auto llvm_or_result = b.CreateOr(llvm_lhs_bool, llvm_rhs_bool);
        // Convert result to i8 for consistency with your implementation needs
        return _zext_i1_to_i8(b, llvm_or_result);
    }

    [[nodiscard]] llvm::Value *_translate_binary_bit_and(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        // Lookup LLVM values for operands
        auto llvm_lhs = _lookup_value(current, b, lhs);
        auto llvm_rhs = _lookup_value(current, b, rhs);
        auto lhs_type = lhs->type();
        auto rhs_type = rhs->type();
        auto elem_type = lhs->type()->is_vector() ? lhs->type()->element() : lhs->type();
        // Type and null checks
        LUISA_ASSERT(lhs_type != nullptr && rhs_type != nullptr, "Operand type is null.");
        LUISA_ASSERT(lhs_type == rhs_type, "Type mismatch for bitwise and.");
        LUISA_ASSERT(lhs_type->is_scalar() || lhs_type->is_vector(), "Invalid operand type.");

        // Perform bitwise AND operation
        switch (elem_type->tag()) {
            case Type::Tag::BOOL: [[fallthrough]];
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::INT64: [[fallthrough]];
            case Type::Tag::UINT8: [[fallthrough]];
            case Type::Tag::UINT16: [[fallthrough]];
            case Type::Tag::UINT32: [[fallthrough]];
            case Type::Tag::UINT64: return b.CreateAnd(llvm_lhs, llvm_rhs);
            default: break;
        }
        LUISA_ERROR_WITH_LOCATION("Invalid binary bit and operand type: {}.", elem_type->description());
    }

    [[nodiscard]] llvm::Value *_translate_binary_bit_or(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        // Lookup LLVM values for operands
        auto llvm_lhs = _lookup_value(current, b, lhs);
        auto llvm_rhs = _lookup_value(current, b, rhs);
        auto lhs_type = lhs->type();
        auto rhs_type = rhs->type();
        auto elem_type = lhs->type()->is_vector() ? lhs->type()->element() : lhs->type();
        // Type and null checks
        LUISA_ASSERT(lhs_type != nullptr && rhs_type != nullptr, "Operand type is null.");
        LUISA_ASSERT(lhs_type == rhs_type, "Type mismatch for bitwise and.");
        LUISA_ASSERT(lhs_type->is_scalar() || lhs_type->is_vector(), "Invalid operand type.");

        // Perform bitwise AND operation
        switch (elem_type->tag()) {
            case Type::Tag::BOOL: [[fallthrough]];
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::INT64: [[fallthrough]];
            case Type::Tag::UINT8: [[fallthrough]];
            case Type::Tag::UINT16: [[fallthrough]];
            case Type::Tag::UINT32: [[fallthrough]];
            case Type::Tag::UINT64: return b.CreateOr(llvm_lhs, llvm_rhs);
            default: break;
        }
        LUISA_ERROR_WITH_LOCATION("Invalid binary bit or operand type: {}.", elem_type->description());
    }

    [[nodiscard]] llvm::Value *_translate_binary_bit_xor(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        // Lookup LLVM values for operands
        auto llvm_lhs = _lookup_value(current, b, lhs);
        auto llvm_rhs = _lookup_value(current, b, rhs);
        auto lhs_type = lhs->type();
        auto rhs_type = rhs->type();
        auto elem_type = lhs->type()->is_vector() ? lhs->type()->element() : lhs->type();
        // Type and null checks
        LUISA_ASSERT(lhs_type != nullptr && rhs_type != nullptr, "Operand type is null.");
        LUISA_ASSERT(lhs_type == rhs_type, "Type mismatch for bitwise and.");
        LUISA_ASSERT(lhs_type->is_scalar() || lhs_type->is_vector(), "Invalid operand type.");

        // Perform bitwise AND operation
        switch (elem_type->tag()) {
            case Type::Tag::BOOL: [[fallthrough]];
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::INT64: [[fallthrough]];
            case Type::Tag::UINT8: [[fallthrough]];
            case Type::Tag::UINT16: [[fallthrough]];
            case Type::Tag::UINT32: [[fallthrough]];
            case Type::Tag::UINT64: return b.CreateXor(llvm_lhs, llvm_rhs);
            default: break;
        }
        LUISA_ERROR_WITH_LOCATION("Invalid binary bit xor operand type: {}.", elem_type->description());
    }

    [[nodiscard]] llvm::Value *_translate_binary_shift_left(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        // Lookup LLVM values for operands
        auto llvm_lhs = _lookup_value(current, b, lhs);
        auto llvm_rhs = _lookup_value(current, b, rhs);
        auto lhs_type = lhs->type();
        auto rhs_type = rhs->type();
        auto elem_type = lhs->type()->is_vector() ? lhs->type()->element() : lhs->type();

        // Type and null checks
        LUISA_ASSERT(lhs_type != nullptr && rhs_type != nullptr, "Operand type is null.");
        LUISA_ASSERT(lhs_type == rhs_type, "Type mismatch for shift left.");
        LUISA_ASSERT(lhs_type->is_scalar() || lhs_type->is_vector(), "Invalid operand type.");

        // Perform shift left operation (only valid for integer types)
        switch (elem_type->tag()) {
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::INT64: [[fallthrough]];
            case Type::Tag::UINT8: [[fallthrough]];
            case Type::Tag::UINT16: [[fallthrough]];
            case Type::Tag::UINT32: [[fallthrough]];
            case Type::Tag::UINT64: return b.CreateShl(llvm_lhs, llvm_rhs);
            default: break;
        }
        LUISA_ERROR_WITH_LOCATION("Invalid operand type for shift left operation: {}.", elem_type->description());
    }

    [[nodiscard]] llvm::Value *_translate_binary_shift_right(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        // Lookup LLVM values for operands
        auto llvm_lhs = _lookup_value(current, b, lhs);
        auto llvm_rhs = _lookup_value(current, b, rhs);
        auto lhs_type = lhs->type();
        auto rhs_type = rhs->type();
        auto elem_type = lhs->type()->is_vector() ? lhs->type()->element() : lhs->type();

        // Type and null checks
        LUISA_ASSERT(lhs_type != nullptr && rhs_type != nullptr, "Operand type is null.");
        LUISA_ASSERT(lhs_type == rhs_type, "Type mismatch for shift left.");
        LUISA_ASSERT(lhs_type->is_scalar() || lhs_type->is_vector(), "Invalid operand type.");

        // Perform shift left operation (only valid for integer types)
        switch (elem_type->tag()) {
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::INT64: return b.CreateAShr(llvm_lhs, llvm_rhs);
            case Type::Tag::UINT8: [[fallthrough]];
            case Type::Tag::UINT16: [[fallthrough]];
            case Type::Tag::UINT32: [[fallthrough]];
            case Type::Tag::UINT64: return b.CreateLShr(llvm_lhs, llvm_rhs);
            default: break;
        }
        LUISA_ERROR_WITH_LOCATION("Invalid operand type for shift left operation: {}.", elem_type->description());
    }

    [[nodiscard]] llvm::Value *_translate_binary_rotate_left(CurrentFunction &current, IRBuilder &b, const xir::Value *value, const xir::Value *shift) noexcept {
        auto llvm_value = _lookup_value(current, b, value);
        auto llvm_shift = _lookup_value(current, b, shift);
        auto value_type = value->type();
        auto elem_type = value_type->is_vector() ? value_type->element() : value_type;
        LUISA_ASSERT(value_type != nullptr, "Operand type is null.");
        LUISA_ASSERT(value_type == shift->type(), "Type mismatch for rotate left.");
        LUISA_ASSERT(value_type->is_scalar() || value_type->is_vector(), "Invalid operand type.");
        auto bit_width = 0u;
        switch (elem_type->tag()) {
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::UINT8: bit_width = 8; break;
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::UINT16: bit_width = 16; break;
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::UINT32: bit_width = 32; break;
            case Type::Tag::INT64: [[fallthrough]];
            case Type::Tag::UINT64: bit_width = 64; break;
            default: LUISA_ERROR_WITH_LOCATION(
                "Invalid operand type for rotate left operation: {}.",
                elem_type->description());
        }
        auto llvm_elem_type = _translate_type(elem_type, false);
        auto llvm_bit_width = llvm::ConstantInt::get(llvm_elem_type, bit_width);
        if (value_type->is_vector()) {
            llvm_bit_width = llvm::ConstantVector::getSplat(
                llvm::ElementCount::getFixed(value_type->dimension()),
                llvm_bit_width);
        }
        auto shifted_left = b.CreateShl(llvm_value, llvm_shift);
        auto complement_shift = b.CreateSub(llvm_bit_width, llvm_shift);
        auto shifted_right = b.CreateLShr(llvm_value, complement_shift);
        return b.CreateOr(shifted_left, shifted_right);
    }

    [[nodiscard]] llvm::Value *_translate_binary_rotate_right(CurrentFunction &current, IRBuilder &b, const xir::Value *value, const xir::Value *shift) noexcept {
        // Lookup LLVM values for operands
        auto llvm_value = _lookup_value(current, b, value);
        auto llvm_shift = _lookup_value(current, b, shift);
        auto value_type = value->type();
        auto elem_type = value_type->is_vector() ? value_type->element() : value_type;

        // Type and null checks
        LUISA_ASSERT(value_type != nullptr, "Operand type is null.");
        LUISA_ASSERT(value_type == shift->type(), "Type mismatch for rotate right.");
        LUISA_ASSERT(value_type->is_scalar() || value_type->is_vector(), "Invalid operand type.");

        auto bit_width = 0u;
        switch (elem_type->tag()) {
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::UINT8: bit_width = 8; break;
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::UINT16: bit_width = 16; break;
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::UINT32: bit_width = 32; break;
            case Type::Tag::INT64: [[fallthrough]];
            case Type::Tag::UINT64: bit_width = 64; break;
            default: LUISA_ERROR_WITH_LOCATION(
                "Invalid operand type for rotate right operation: {}.",
                elem_type->description());
        }
        auto llvm_elem_type = _translate_type(elem_type, false);
        auto llvm_bit_width = llvm::ConstantInt::get(llvm_elem_type, bit_width);
        if (value_type->is_vector()) {
            llvm_bit_width = llvm::ConstantVector::getSplat(
                llvm::ElementCount::getFixed(value_type->dimension()),
                llvm_bit_width);
        }
        auto shifted_right = b.CreateLShr(llvm_value, llvm_shift);
        auto complement_shift = b.CreateSub(llvm_bit_width, llvm_shift);
        auto shifted_left = b.CreateShl(llvm_value, complement_shift);
        return b.CreateOr(shifted_left, shifted_right);
    }

    [[nodiscard]] llvm::Value *_translate_binary_less(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        // Lookup LLVM values for operands
        auto llvm_lhs = _lookup_value(current, b, lhs);
        auto llvm_rhs = _lookup_value(current, b, rhs);
        auto lhs_type = lhs->type();
        auto rhs_type = rhs->type();
        auto elem_type = lhs->type()->is_vector() ? lhs->type()->element() : lhs->type();

        // Type and null checks
        LUISA_ASSERT(lhs_type != nullptr && rhs_type != nullptr, "Operand type is null.");
        LUISA_ASSERT(lhs_type == rhs_type, "Type mismatch for binary less.");
        LUISA_ASSERT(lhs_type->is_scalar() || lhs_type->is_vector(), "Invalid operand type.");

        // Perform less-than comparison based on the type
        llvm::Value *result = nullptr;
        switch (elem_type->tag()) {
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::INT64: result = b.CreateICmpSLT(llvm_lhs, llvm_rhs); break;// Signed integer less-than comparison
            case Type::Tag::UINT8: [[fallthrough]];
            case Type::Tag::UINT16: [[fallthrough]];
            case Type::Tag::UINT32: [[fallthrough]];
            case Type::Tag::UINT64: result = b.CreateICmpULT(llvm_lhs, llvm_rhs); break;// Unsigned integer less-than comparison
            case Type::Tag::FLOAT16: [[fallthrough]];
            case Type::Tag::FLOAT32: [[fallthrough]];
            case Type::Tag::FLOAT64: result = b.CreateFCmpOLT(llvm_lhs, llvm_rhs); break;// Floating-point unordered less-than comparison
            default: LUISA_ERROR_WITH_LOCATION("Invalid operand type for binary less-equal operation: {}.", elem_type->description());
        }
        return _zext_i1_to_i8(b, result);
    }

    [[nodiscard]] llvm::Value *_translate_binary_greater(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        auto llvm_lhs = _lookup_value(current, b, lhs);
        auto llvm_rhs = _lookup_value(current, b, rhs);
        auto lhs_type = lhs->type();
        auto rhs_type = rhs->type();
        auto elem_type = lhs->type()->is_vector() ? lhs->type()->element() : lhs->type();

        LUISA_ASSERT(lhs_type != nullptr && rhs_type != nullptr, "Operand type is null.");
        LUISA_ASSERT(lhs_type == rhs_type, "Type mismatch for binary greater.");
        LUISA_ASSERT(lhs_type->is_scalar() || lhs_type->is_vector(), "Invalid operand type.");

        llvm::Value *result = nullptr;
        switch (elem_type->tag()) {
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::INT64: result = b.CreateICmpSGT(llvm_lhs, llvm_rhs); break;// Signed integer greater-than
            case Type::Tag::UINT8: [[fallthrough]];
            case Type::Tag::UINT16: [[fallthrough]];
            case Type::Tag::UINT32: [[fallthrough]];
            case Type::Tag::UINT64: result = b.CreateICmpUGT(llvm_lhs, llvm_rhs); break;// Unsigned integer greater-than
            case Type::Tag::FLOAT16: [[fallthrough]];
            case Type::Tag::FLOAT32: [[fallthrough]];
            case Type::Tag::FLOAT64: result = b.CreateFCmpOGT(llvm_lhs, llvm_rhs); break;// Ordered greater-than
            default: LUISA_ERROR_WITH_LOCATION("Invalid operand type for binary less-equal operation: {}.", elem_type->description());
        }
        return _zext_i1_to_i8(b, result);
    }

    [[nodiscard]] llvm::Value *_translate_binary_less_equal(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        auto llvm_lhs = _lookup_value(current, b, lhs);
        auto llvm_rhs = _lookup_value(current, b, rhs);
        auto lhs_type = lhs->type();
        auto rhs_type = rhs->type();
        auto elem_type = lhs->type()->is_vector() ? lhs->type()->element() : lhs->type();

        LUISA_ASSERT(lhs_type != nullptr && rhs_type != nullptr, "Operand type is null.");
        LUISA_ASSERT(lhs_type == rhs_type, "Type mismatch for binary less-equal.");
        LUISA_ASSERT(lhs_type->is_scalar() || lhs_type->is_vector(), "Invalid operand type.");

        llvm::Value *result = nullptr;
        switch (elem_type->tag()) {
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::INT64: result = b.CreateICmpSLE(llvm_lhs, llvm_rhs); break;// Signed integer less-than-or-equal
            case Type::Tag::UINT8: [[fallthrough]];
            case Type::Tag::UINT16: [[fallthrough]];
            case Type::Tag::UINT32: [[fallthrough]];
            case Type::Tag::UINT64: result = b.CreateICmpULE(llvm_lhs, llvm_rhs); break;// Unsigned integer less-than-or-equal
            case Type::Tag::FLOAT16: [[fallthrough]];
            case Type::Tag::FLOAT32: [[fallthrough]];
            case Type::Tag::FLOAT64: result = b.CreateFCmpOLE(llvm_lhs, llvm_rhs); break;// Ordered less-than-or-equal
            default: LUISA_ERROR_WITH_LOCATION("Invalid operand type for binary less-equal operation: {}.", elem_type->description());
        }
        return _zext_i1_to_i8(b, result);
    }

    [[nodiscard]] llvm::Value *_translate_binary_greater_equal(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        auto llvm_lhs = _lookup_value(current, b, lhs);
        auto llvm_rhs = _lookup_value(current, b, rhs);
        auto lhs_type = lhs->type();
        auto rhs_type = rhs->type();
        auto elem_type = lhs->type()->is_vector() ? lhs->type()->element() : lhs->type();

        LUISA_ASSERT(lhs_type != nullptr && rhs_type != nullptr, "Operand type is null.");
        LUISA_ASSERT(lhs_type == rhs_type, "Type mismatch for binary greater-equal.");
        LUISA_ASSERT(lhs_type->is_scalar() || lhs_type->is_vector(), "Invalid operand type.");

        llvm::Value *result = nullptr;
        switch (elem_type->tag()) {
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::INT64: result = b.CreateICmpSGE(llvm_lhs, llvm_rhs); break;// Signed integer greater-than-or-equal
            case Type::Tag::UINT8: [[fallthrough]];
            case Type::Tag::UINT16: [[fallthrough]];
            case Type::Tag::UINT32: [[fallthrough]];
            case Type::Tag::UINT64: result = b.CreateICmpUGE(llvm_lhs, llvm_rhs); break;// Unsigned integer greater-than-or-equal
            case Type::Tag::FLOAT16: [[fallthrough]];
            case Type::Tag::FLOAT32: [[fallthrough]];
            case Type::Tag::FLOAT64: result = b.CreateFCmpOGE(llvm_lhs, llvm_rhs); break;// Ordered greater-than-or-equal
            default: LUISA_ERROR_WITH_LOCATION("Invalid operand type for binary less-equal operation: {}.", elem_type->description());
        }
        return _zext_i1_to_i8(b, result);
    }

    [[nodiscard]] llvm::Value *_translate_binary_equal(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        auto llvm_lhs = _lookup_value(current, b, lhs);
        auto llvm_rhs = _lookup_value(current, b, rhs);
        auto lhs_type = lhs->type();
        auto rhs_type = rhs->type();
        auto elem_type = lhs->type()->is_vector() ? lhs->type()->element() : lhs->type();

        LUISA_ASSERT(lhs_type != nullptr && rhs_type != nullptr, "Operand type is null.");
        LUISA_ASSERT(lhs_type == rhs_type, "Type mismatch for binary equal.");
        LUISA_ASSERT(lhs_type->is_scalar() || lhs_type->is_vector(), "Invalid operand type.");

        llvm::Value *result = nullptr;
        switch (elem_type->tag()) {
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::INT64: [[fallthrough]];
            case Type::Tag::UINT8: [[fallthrough]];
            case Type::Tag::UINT16: [[fallthrough]];
            case Type::Tag::UINT32: [[fallthrough]];
            case Type::Tag::UINT64: result = b.CreateICmpEQ(llvm_lhs, llvm_rhs); break;// Integer equality comparison
            case Type::Tag::FLOAT16: [[fallthrough]];
            case Type::Tag::FLOAT32: [[fallthrough]];
            case Type::Tag::FLOAT64: result = b.CreateFCmpOEQ(llvm_lhs, llvm_rhs); break;// Ordered equality comparison
            default: LUISA_ERROR_WITH_LOCATION("Invalid operand type for binary less-equal operation: {}.", elem_type->description());
        }
        return _zext_i1_to_i8(b, result);
    }

    [[nodiscard]] llvm::Value *_translate_binary_not_equal(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        auto llvm_lhs = _lookup_value(current, b, lhs);
        auto llvm_rhs = _lookup_value(current, b, rhs);
        auto lhs_type = lhs->type();
        auto rhs_type = rhs->type();
        auto elem_type = lhs->type()->is_vector() ? lhs->type()->element() : lhs->type();

        LUISA_ASSERT(lhs_type != nullptr && rhs_type != nullptr, "Operand type is null.");
        LUISA_ASSERT(lhs_type == rhs_type, "Type mismatch for binary equal.");
        LUISA_ASSERT(lhs_type->is_scalar() || lhs_type->is_vector(), "Invalid operand type.");

        llvm::Value *result = nullptr;
        switch (elem_type->tag()) {
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::INT64: [[fallthrough]];
            case Type::Tag::UINT8: [[fallthrough]];
            case Type::Tag::UINT16: [[fallthrough]];
            case Type::Tag::UINT32: [[fallthrough]];
            case Type::Tag::UINT64: result = b.CreateICmpNE(llvm_lhs, llvm_rhs); break;// Integer equality comparison
            case Type::Tag::FLOAT16: [[fallthrough]];
            case Type::Tag::FLOAT32: [[fallthrough]];
            case Type::Tag::FLOAT64: result = b.CreateFCmpONE(llvm_lhs, llvm_rhs); break;// Ordered equality comparison
            default: LUISA_ERROR_WITH_LOCATION("Invalid operand type for binary less-equal operation: {}.", elem_type->description());
        }
        return _zext_i1_to_i8(b, result);
    }

    [[nodiscard]] llvm::Value *_translate_unary_fp_math_operation(CurrentFunction &current, IRBuilder &b, const xir::Value *operand, llvm::Intrinsic::ID intrinsic_id) noexcept {
        // Lookup LLVM value for operand
        auto llvm_operand = _lookup_value(current, b, operand);
        auto operand_type = operand->type();

        // Type and null checks
        LUISA_ASSERT(operand_type != nullptr, "Operand type is null.");
        LUISA_ASSERT(operand_type->is_scalar() || operand_type->is_vector(), "Invalid operand type.");

        // Check if the operand is a valid floating-point type
        auto elem_type = operand_type->is_vector() ? operand_type->element() : operand_type;
        switch (elem_type->tag()) {
            case Type::Tag::FLOAT16: [[fallthrough]];
            case Type::Tag::FLOAT32: [[fallthrough]];
            case Type::Tag::FLOAT64:
                // Use LLVM's intrinsic function based on the provided intrinsic ID
                return b.CreateUnaryIntrinsic(intrinsic_id, llvm_operand);
            default:
                break;
        }
        LUISA_ERROR_WITH_LOCATION("Invalid operand type for unary math operation {}: {}.",
                                  intrinsic_id, operand_type->description());
    }

    [[nodiscard]] llvm::Value *_translate_binary_fp_math_operation(CurrentFunction &current, IRBuilder &b,
                                                                   const xir::Value *op0, const xir::Value *op1,
                                                                   llvm::Intrinsic::ID intrinsic_id) noexcept {
        auto llvm_op0 = _lookup_value(current, b, op0);
        auto llvm_op1 = _lookup_value(current, b, op1);
        auto op0_type = op0->type();
        auto op1_type = op1->type();
        LUISA_ASSERT(op0_type == op1_type, "Type mismatch.");
        LUISA_ASSERT(op0_type != nullptr && (op0_type->is_scalar() || op0_type->is_vector()), "Invalid operand type.");
        auto elem_type = op0_type->is_vector() ? op0_type->element() : op0_type;
        switch (elem_type->tag()) {
            case Type::Tag::FLOAT16: [[fallthrough]];
            case Type::Tag::FLOAT32: [[fallthrough]];
            case Type::Tag::FLOAT64:
                return b.CreateBinaryIntrinsic(intrinsic_id, llvm_op0, llvm_op1);
            default:
                break;
        }
        LUISA_ERROR_WITH_LOCATION("Invalid operand type for binary math operation {}: {}.",
                                  intrinsic_id, op0_type->description());
    }

    [[nodiscard]] llvm::Value *_translate_vector_reduce(CurrentFunction &current, IRBuilder &b,
                                                        xir::IntrinsicOp op, const xir::Value *operand) noexcept {
        LUISA_ASSERT(operand->type() != nullptr && operand->type()->is_vector(),
                     "Invalid operand type for reduce_sum operation.");
        auto operand_elem_type = operand->type()->element();
        auto llvm_operand = _lookup_value(current, b, operand);
        switch (operand->type()->element()->tag()) {
            case Type::Tag::INT8: [[fallthrough]];
            case Type::Tag::UINT8: [[fallthrough]];
            case Type::Tag::INT16: [[fallthrough]];
            case Type::Tag::UINT16: [[fallthrough]];
            case Type::Tag::INT32: [[fallthrough]];
            case Type::Tag::UINT32: [[fallthrough]];
            case Type::Tag::INT64: [[fallthrough]];
            case Type::Tag::UINT64: {
                switch (op) {
                    case xir::IntrinsicOp::REDUCE_SUM: return b.CreateAddReduce(llvm_operand);
                    case xir::IntrinsicOp::REDUCE_PRODUCT: return b.CreateMulReduce(llvm_operand);
                    case xir::IntrinsicOp::REDUCE_MIN: return b.CreateIntMinReduce(llvm_operand);
                    case xir::IntrinsicOp::REDUCE_MAX: return b.CreateIntMaxReduce(llvm_operand);
                    default: break;
                }
                LUISA_ERROR_WITH_LOCATION("Invalid reduce operation: {}.", xir::to_string(op));
            }
            case Type::Tag::FLOAT16: [[fallthrough]];
            case Type::Tag::FLOAT32: [[fallthrough]];
            case Type::Tag::FLOAT64: {
                auto llvm_elem_type = _translate_type(operand_elem_type, false);
                auto llvm_zero = llvm::ConstantFP::get(llvm_elem_type, 0.);
                auto llvm_one = llvm::ConstantFP::get(llvm_elem_type, 1.);
                switch (op) {
                    case xir::IntrinsicOp::REDUCE_SUM: return b.CreateFAddReduce(llvm_zero, llvm_operand);
                    case xir::IntrinsicOp::REDUCE_PRODUCT: return b.CreateFMulReduce(llvm_one, llvm_operand);
                    case xir::IntrinsicOp::REDUCE_MIN: return b.CreateFPMinReduce(llvm_operand);
                    case xir::IntrinsicOp::REDUCE_MAX: return b.CreateFPMaxReduce(llvm_operand);
                    default: break;
                }
                LUISA_ERROR_WITH_LOCATION("Invalid reduce operation: {}.", xir::to_string(op));
            }
            default: break;
        }
        LUISA_ERROR_WITH_LOCATION("Invalid operand type for reduce_sum operation: {}.",
                                  operand_elem_type->description());
    }

    [[nodiscard]] llvm::Value *_translate_vector_dot(CurrentFunction &current, IRBuilder &b, const xir::Value *lhs, const xir::Value *rhs) noexcept {
        auto llvm_mul = _translate_binary_mul(current, b, lhs, rhs);
        if (llvm_mul->getType()->isFPOrFPVectorTy()) {
            auto llvm_elem_type = llvm_mul->getType()->isVectorTy() ?
                                      llvm::cast<llvm::VectorType>(llvm_mul->getType())->getElementType() :
                                      llvm_mul->getType();
            auto llvm_zero = llvm::ConstantFP::get(llvm_elem_type, 0.);
            return b.CreateFAddReduce(llvm_zero, llvm_mul);
        }
        return b.CreateAddReduce(llvm_mul);
    }

    [[nodiscard]] llvm::Value *_translate_isinf_isnan(CurrentFunction &current, IRBuilder &b, xir::IntrinsicOp op, const xir::Value *x) noexcept {
        auto type = x->type();
        auto elem_type = type->is_vector() ? type->element() : type;
        auto llvm_x = _lookup_value(current, b, x);
        auto [llvm_mask, llvm_test] = [&] {
            switch (elem_type->tag()) {
                case Type::Tag::FLOAT16: return std::make_pair(
                    llvm::cast<llvm::Constant>(b.getInt16(0x7fffu)),
                    llvm::cast<llvm::Constant>(b.getInt16(0x7c00u)));
                case Type::Tag::FLOAT32: return std::make_pair(
                    llvm::cast<llvm::Constant>(b.getInt32(0x7fffffffu)),
                    llvm::cast<llvm::Constant>(b.getInt32(0x7f800000u)));
                case Type::Tag::FLOAT64: return std::make_pair(
                    llvm::cast<llvm::Constant>(b.getInt64(0x7fffffffffffffffull)),
                    llvm::cast<llvm::Constant>(b.getInt64(0x7ff0000000000000ull)));
                default: break;
            }
            LUISA_ERROR_WITH_LOCATION("Invalid operand type for isinf/isnan operation: {}.", type->description());
        }();
        if (type->is_vector()) {
            auto dim = llvm::ElementCount::getFixed(type->dimension());
            llvm_mask = llvm::ConstantVector::getSplat(dim, llvm_mask);
            llvm_test = llvm::ConstantVector::getSplat(dim, llvm_test);
        }
        auto llvm_int_type = llvm_mask->getType();
        auto llvm_bits = b.CreateBitCast(llvm_x, llvm_int_type);
        auto llvm_and = b.CreateAnd(llvm_bits, llvm_mask);
        LUISA_DEBUG_ASSERT(op == xir::IntrinsicOp::ISINF || op == xir::IntrinsicOp::ISNAN,
                           "Unexpected intrinsic operation.");
        auto llvm_cmp = op == xir::IntrinsicOp::ISINF ?
                            b.CreateICmpEQ(llvm_and, llvm_test) :
                            b.CreateICmpUGE(llvm_and, llvm_test);
        return _zext_i1_to_i8(b, llvm_cmp);
    }

    [[nodiscard]] llvm::Value *_translate_buffer_write(CurrentFunction &current, IRBuilder &b, const xir::IntrinsicInst *inst) noexcept {
        //LUISA_NOT_IMPLEMENTED();
        auto buffer = inst->operand(0u);
        auto slot = inst->operand(1u);
        auto value = inst->operand(2u);
        auto llvm_buffer = _lookup_value(current, b, buffer);// Get the buffer pointer
        auto llvm_slot = _lookup_value(current, b, slot);    // Get the slot index
        auto llvm_value = _lookup_value(current, b, value);  // Get the value to write
        auto element_type = llvm_value->getType();           // Type of the value being written
        auto target_address = b.CreateInBoundsGEP(
            element_type,// Element type
            llvm_buffer, // Base pointer
            llvm_slot    // Index
        );
        return b.CreateAlignedStore(llvm_value, target_address, llvm::MaybeAlign(value->type()->alignment()));
    }

    [[nodiscard]] llvm::Value *_translate_buffer_read(CurrentFunction &current, IRBuilder &b, const xir::IntrinsicInst *inst) noexcept {
        //LUISA_NOT_IMPLEMENTED();
        auto buffer = inst->operand(0u);
        auto slot = inst->operand(1u);
        auto llvm_buffer = _lookup_value(current, b, buffer);   // Get the buffer pointer
        auto llvm_slot = _lookup_value(current, b, slot);       // Get the slot index
        auto element_type = _translate_type(inst->type(), true);// Type of the value being read
        auto target_address = b.CreateInBoundsGEP(
            element_type,// Element type
            llvm_buffer, // Base pointer
            llvm_slot    // Index
        );
        return b.CreateAlignedLoad(element_type, target_address, llvm::MaybeAlign(inst->type()->alignment()));
    }

    [[nodiscard]] llvm::Value *_translate_intrinsic_inst(CurrentFunction &current, IRBuilder &b, const xir::IntrinsicInst *inst) noexcept {
        switch (inst->op()) {
            case xir::IntrinsicOp::NOP: LUISA_ERROR_WITH_LOCATION("Unexpected NOP.");
            case xir::IntrinsicOp::UNARY_PLUS: return _translate_unary_plus(current, b, inst->operand(0u));
            case xir::IntrinsicOp::UNARY_MINUS: return _translate_unary_minus(current, b, inst->operand(0u));
            case xir::IntrinsicOp::UNARY_LOGIC_NOT: return _translate_unary_logic_not(current, b, inst->operand(0u));
            case xir::IntrinsicOp::UNARY_BIT_NOT: return _translate_unary_bit_not(current, b, inst->operand(0u));
            case xir::IntrinsicOp::BINARY_ADD: return _translate_binary_add(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_SUB: return _translate_binary_sub(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_MUL: return _translate_binary_mul(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_DIV: return _translate_binary_div(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_MOD: return _translate_binary_mod(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_LOGIC_AND: return _translate_binary_logic_and(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_LOGIC_OR: return _translate_binary_logic_or(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_BIT_AND: return _translate_binary_bit_and(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_BIT_OR: return _translate_binary_bit_or(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_BIT_XOR: return _translate_binary_bit_xor(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_SHIFT_LEFT: return _translate_binary_shift_left(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_SHIFT_RIGHT: return _translate_binary_shift_right(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_ROTATE_LEFT: return _translate_binary_rotate_left(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_ROTATE_RIGHT: return _translate_binary_rotate_right(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_LESS: return _translate_binary_less(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_GREATER: return _translate_binary_greater(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_LESS_EQUAL: return _translate_binary_less_equal(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_GREATER_EQUAL: return _translate_binary_greater_equal(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_EQUAL: return _translate_binary_equal(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::BINARY_NOT_EQUAL: return _translate_binary_not_equal(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::THREAD_ID: return current.builtin_variables[CurrentFunction::builtin_variable_index_thread_id];
            case xir::IntrinsicOp::BLOCK_ID: return current.builtin_variables[CurrentFunction::builtin_variable_index_block_id];
            case xir::IntrinsicOp::WARP_LANE_ID: return llvm::ConstantInt::get(b.getInt32Ty(), 0);// CPU only has one lane
            case xir::IntrinsicOp::DISPATCH_ID: return current.builtin_variables[CurrentFunction::builtin_variable_index_dispatch_id];
            case xir::IntrinsicOp::KERNEL_ID: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::OBJECT_ID: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::BLOCK_SIZE: return current.builtin_variables[CurrentFunction::builtin_variable_index_block_size];
            case xir::IntrinsicOp::WARP_SIZE: return llvm::ConstantInt::get(b.getInt32Ty(), 1);// CPU only has one lane
            case xir::IntrinsicOp::DISPATCH_SIZE: return current.builtin_variables[CurrentFunction::builtin_variable_index_dispatch_size];
            case xir::IntrinsicOp::SYNCHRONIZE_BLOCK: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::ALL: {
                auto llvm_operand = _lookup_value(current, b, inst->operand(0u));
                return b.CreateAndReduce(llvm_operand);
            }
            case xir::IntrinsicOp::ANY: {
                auto llvm_operand = _lookup_value(current, b, inst->operand(0u));
                return b.CreateOrReduce(llvm_operand);
            }
            case xir::IntrinsicOp::SELECT: {
                // note that the order of operands is reversed
                auto llvm_false_value = _lookup_value(current, b, inst->operand(0u));
                auto llvm_true_value = _lookup_value(current, b, inst->operand(1u));
                auto llvm_condition = _lookup_value(current, b, inst->operand(2u));
                auto llvm_zero = llvm::Constant::getNullValue(llvm_condition->getType());
                auto llvm_cmp = b.CreateICmpNE(llvm_condition, llvm_zero);
                return b.CreateSelect(llvm_cmp, llvm_true_value, llvm_false_value);
            }
            case xir::IntrinsicOp::CLAMP: {
                // clamp(x, lb, ub) = min(max(x, lb), ub)
                auto x_type = inst->operand(0u)->type();
                auto lb_type = inst->operand(1u)->type();
                auto ub_type = inst->operand(2u)->type();
                LUISA_ASSERT(x_type != nullptr && x_type == lb_type && x_type == ub_type, "Type mismatch for clamp.");
                auto llvm_x = _lookup_value(current, b, inst->operand(0u));
                auto llvm_lb = _lookup_value(current, b, inst->operand(1u));
                auto llvm_ub = _lookup_value(current, b, inst->operand(2u));
                auto elem_type = x_type->is_vector() ? x_type->element() : x_type;
                switch (elem_type->tag()) {
                    case Type::Tag::INT8: [[fallthrough]];
                    case Type::Tag::INT16: [[fallthrough]];
                    case Type::Tag::INT32: [[fallthrough]];
                    case Type::Tag::INT64: {
                        auto llvm_max = b.CreateBinaryIntrinsic(llvm::Intrinsic::smax, llvm_x, llvm_lb);
                        return b.CreateBinaryIntrinsic(llvm::Intrinsic::smin, llvm_max, llvm_ub);
                    }
                    case Type::Tag::UINT8: [[fallthrough]];
                    case Type::Tag::UINT16: [[fallthrough]];
                    case Type::Tag::UINT32: [[fallthrough]];
                    case Type::Tag::UINT64: {
                        auto llvm_max = b.CreateBinaryIntrinsic(llvm::Intrinsic::umax, llvm_x, llvm_lb);
                        return b.CreateBinaryIntrinsic(llvm::Intrinsic::umin, llvm_max, llvm_ub);
                    }
                    case Type::Tag::FLOAT16: [[fallthrough]];
                    case Type::Tag::FLOAT32: [[fallthrough]];
                    case Type::Tag::FLOAT64: {
                        auto llvm_max = b.CreateBinaryIntrinsic(llvm::Intrinsic::maxnum, llvm_x, llvm_lb);
                        return b.CreateBinaryIntrinsic(llvm::Intrinsic::minnum, llvm_max, llvm_ub);
                    }
                    default: break;
                }
                LUISA_ERROR_WITH_LOCATION("Invalid operand type for clamp operation: {}.", x_type->description());
            }
            case xir::IntrinsicOp::SATURATE: {
                // saturate(x) = min(max(x, 0), 1)
                auto llvm_x = _lookup_value(current, b, inst->operand(0u));
                LUISA_ASSERT(llvm_x->getType()->isFPOrFPVectorTy(), "Invalid operand type for saturate operation.");
                auto llvm_zero = llvm::ConstantFP::get(llvm_x->getType(), 0.);
                auto llvm_one = llvm::ConstantFP::get(llvm_x->getType(), 1.);
                auto llvm_max = b.CreateBinaryIntrinsic(llvm::Intrinsic::maxnum, llvm_x, llvm_zero);
                return b.CreateBinaryIntrinsic(llvm::Intrinsic::minnum, llvm_max, llvm_one);
            }
            case xir::IntrinsicOp::LERP: {
                // lerp(a, b, t) = (b - a) * t + a = fma(t, b - a, a)
                auto va_type = inst->operand(0u)->type();
                auto vb_type = inst->operand(1u)->type();
                auto t_type = inst->operand(2u)->type();
                LUISA_ASSERT(va_type != nullptr && va_type == vb_type && va_type == t_type, "Type mismatch for lerp.");
                auto llvm_va = _lookup_value(current, b, inst->operand(0u));
                auto llvm_vb = _lookup_value(current, b, inst->operand(1u));
                auto llvm_t = _lookup_value(current, b, inst->operand(2u));
                LUISA_ASSERT(llvm_va->getType()->isFPOrFPVectorTy(), "Invalid operand type for lerp operation.");
                auto llvm_diff = b.CreateFSub(llvm_vb, llvm_va);
                return b.CreateIntrinsic(llvm_va->getType(), llvm::Intrinsic::fma, {llvm_t, llvm_diff, llvm_va});
            }
            case xir::IntrinsicOp::SMOOTHSTEP: {
                // smoothstep(edge0, edge1, x) = t * t * (3.0 - 2.0 * t), where t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0)
                auto type = inst->type();
                LUISA_ASSERT(type != nullptr &&
                                 type == inst->operand(0u)->type() &&
                                 type == inst->operand(1u)->type() &&
                                 type == inst->operand(2u)->type(),
                             "Type mismatch for smoothstep.");
                auto elem_type = type->is_vector() ? type->element() : type;
                LUISA_ASSERT(elem_type->is_float16() || elem_type->is_float32() || elem_type->is_float64(),
                             "Invalid operand type for smoothstep operation.");
                auto llvm_edge0 = _lookup_value(current, b, inst->operand(0u));
                auto llvm_edge1 = _lookup_value(current, b, inst->operand(1u));
                auto llvm_x = _lookup_value(current, b, inst->operand(2u));
                // constant 0, 1, -2, and 3
                auto llvm_elem_type = _translate_type(elem_type, false);
                auto llvm_zero = llvm::ConstantFP::get(llvm_elem_type, 0.);
                auto llvm_one = llvm::ConstantFP::get(llvm_elem_type, 1.);
                auto llvm_minus_two = llvm::ConstantFP::get(llvm_elem_type, -2.);
                auto llvm_three = llvm::ConstantFP::get(llvm_elem_type, 3.);
                if (type->is_vector()) {
                    auto dim = llvm::ElementCount::getFixed(type->dimension());
                    llvm_zero = llvm::ConstantVector::getSplat(dim, llvm_zero);
                    llvm_one = llvm::ConstantVector::getSplat(dim, llvm_one);
                    llvm_minus_two = llvm::ConstantVector::getSplat(dim, llvm_minus_two);
                    llvm_three = llvm::ConstantVector::getSplat(dim, llvm_three);
                }
                // t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0)
                auto llvm_x_minus_edge0 = b.CreateFSub(llvm_x, llvm_edge0);
                auto llvm_edge1_minus_edge0 = b.CreateFSub(llvm_edge1, llvm_edge0);
                auto llvm_t = b.CreateFDiv(llvm_x_minus_edge0, llvm_edge1_minus_edge0);
                // clamp t
                llvm_t = b.CreateBinaryIntrinsic(llvm::Intrinsic::maxnum, llvm_zero, llvm_t);
                llvm_t = b.CreateBinaryIntrinsic(llvm::Intrinsic::minnum, llvm_one, llvm_t);
                // smoothstep
                auto llvm_tt = b.CreateFMul(llvm_t, llvm_t);
                auto llvm_three_minus_two_t = b.CreateIntrinsic(llvm_t->getType(), llvm::Intrinsic::fma, {llvm_minus_two, llvm_t, llvm_three});
                return b.CreateFMul(llvm_tt, llvm_three_minus_two_t);
            }
            case xir::IntrinsicOp::STEP: {
                // step(edge, x) = x < edge ? 0 : 1 = uitofp(x >= edge)
                auto llvm_edge = _lookup_value(current, b, inst->operand(0u));
                auto llvm_x = _lookup_value(current, b, inst->operand(1u));
                auto llvm_cmp = b.CreateFCmpOGE(llvm_x, llvm_edge);
                return b.CreateUIToFP(llvm_cmp, llvm_x->getType());
            }
            case xir::IntrinsicOp::ABS: {
                auto llvm_x = _lookup_value(current, b, inst->operand(0u));
                auto llvm_intrinsic = llvm_x->getType()->isFPOrFPVectorTy() ?
                                          llvm::Intrinsic::fabs :
                                          llvm::Intrinsic::abs;
                return b.CreateUnaryIntrinsic(llvm_intrinsic, llvm_x);
            }
            case xir::IntrinsicOp::MIN: {
                auto llvm_x = _lookup_value(current, b, inst->operand(0u));
                auto llvm_y = _lookup_value(current, b, inst->operand(1u));
                auto elem_type = inst->type()->is_vector() ? inst->type()->element() : inst->type();
                switch (elem_type->tag()) {
                    case Type::Tag::INT8: [[fallthrough]];
                    case Type::Tag::INT16: [[fallthrough]];
                    case Type::Tag::INT32: [[fallthrough]];
                    case Type::Tag::INT64: return b.CreateBinaryIntrinsic(llvm::Intrinsic::smin, llvm_x, llvm_y);
                    case Type::Tag::UINT8: [[fallthrough]];
                    case Type::Tag::UINT16: [[fallthrough]];
                    case Type::Tag::UINT32: [[fallthrough]];
                    case Type::Tag::UINT64: return b.CreateBinaryIntrinsic(llvm::Intrinsic::umin, llvm_x, llvm_y);
                    case Type::Tag::FLOAT16: [[fallthrough]];
                    case Type::Tag::FLOAT32: [[fallthrough]];
                    case Type::Tag::FLOAT64: return b.CreateBinaryIntrinsic(llvm::Intrinsic::minnum, llvm_x, llvm_y);
                    default: break;
                }
                LUISA_ERROR_WITH_LOCATION("Invalid operand type for min operation: {}.",
                                          inst->type()->description());
            }
            case xir::IntrinsicOp::MAX: {
                auto llvm_x = _lookup_value(current, b, inst->operand(0u));
                auto llvm_y = _lookup_value(current, b, inst->operand(1u));
                auto elem_type = inst->type()->is_vector() ? inst->type()->element() : inst->type();
                switch (elem_type->tag()) {
                    case Type::Tag::INT8: [[fallthrough]];
                    case Type::Tag::INT16: [[fallthrough]];
                    case Type::Tag::INT32: [[fallthrough]];
                    case Type::Tag::INT64: return b.CreateBinaryIntrinsic(llvm::Intrinsic::smax, llvm_x, llvm_y);
                    case Type::Tag::UINT8: [[fallthrough]];
                    case Type::Tag::UINT16: [[fallthrough]];
                    case Type::Tag::UINT32: [[fallthrough]];
                    case Type::Tag::UINT64: return b.CreateBinaryIntrinsic(llvm::Intrinsic::umax, llvm_x, llvm_y);
                    case Type::Tag::FLOAT16: [[fallthrough]];
                    case Type::Tag::FLOAT32: [[fallthrough]];
                    case Type::Tag::FLOAT64: return b.CreateBinaryIntrinsic(llvm::Intrinsic::maxnum, llvm_x, llvm_y);
                    default: break;
                }
                LUISA_ERROR_WITH_LOCATION("Invalid operand type for max operation: {}.",
                                          inst->type()->description());
            }
            case xir::IntrinsicOp::CLZ: {
                auto llvm_x = _lookup_value(current, b, inst->operand(0u));
                return b.CreateUnaryIntrinsic(llvm::Intrinsic::ctlz, llvm_x);
            }
            case xir::IntrinsicOp::CTZ: {
                auto llvm_x = _lookup_value(current, b, inst->operand(0u));
                return b.CreateUnaryIntrinsic(llvm::Intrinsic::cttz, llvm_x);
            }
            case xir::IntrinsicOp::POPCOUNT: {
                auto llvm_x = _lookup_value(current, b, inst->operand(0u));
                return b.CreateUnaryIntrinsic(llvm::Intrinsic::ctpop, llvm_x);
            }
            case xir::IntrinsicOp::REVERSE: {
                auto llvm_x = _lookup_value(current, b, inst->operand(0u));
                return b.CreateUnaryIntrinsic(llvm::Intrinsic::bitreverse, llvm_x);
            }
            case xir::IntrinsicOp::ISINF: return _translate_isinf_isnan(current, b, inst->op(), inst->operand(0u));
            case xir::IntrinsicOp::ISNAN: return _translate_isinf_isnan(current, b, inst->op(), inst->operand(0u));
            case xir::IntrinsicOp::ACOS: break;
            case xir::IntrinsicOp::ACOSH: break;
            case xir::IntrinsicOp::ASIN: break;
            case xir::IntrinsicOp::ASINH: break;
            case xir::IntrinsicOp::ATAN: break;
            case xir::IntrinsicOp::ATAN2: break;
            case xir::IntrinsicOp::ATANH: break;
            case xir::IntrinsicOp::COS: return _translate_unary_fp_math_operation(current, b, inst->operand(0u), llvm::Intrinsic::cos);
            case xir::IntrinsicOp::COSH: break;
            case xir::IntrinsicOp::SIN: return _translate_unary_fp_math_operation(current, b, inst->operand(0u), llvm::Intrinsic::sin);
            case xir::IntrinsicOp::SINH: break;
            case xir::IntrinsicOp::TAN: {
                auto llvm_sin = _translate_unary_fp_math_operation(current, b, inst->operand(0u), llvm::Intrinsic::sin);
                auto llvm_cos = _translate_unary_fp_math_operation(current, b, inst->operand(0u), llvm::Intrinsic::cos);
                return b.CreateFDiv(llvm_sin, llvm_cos);
            }
            case xir::IntrinsicOp::TANH: break;
            case xir::IntrinsicOp::EXP: return _translate_unary_fp_math_operation(current, b, inst->operand(0u), llvm::Intrinsic::exp);
            case xir::IntrinsicOp::EXP2: return _translate_unary_fp_math_operation(current, b, inst->operand(0u), llvm::Intrinsic::exp2);
            case xir::IntrinsicOp::EXP10: return _translate_unary_fp_math_operation(current, b, inst->operand(0u), llvm::Intrinsic::exp10);
            case xir::IntrinsicOp::LOG: return _translate_unary_fp_math_operation(current, b, inst->operand(0u), llvm::Intrinsic::log);
            case xir::IntrinsicOp::LOG2: return _translate_unary_fp_math_operation(current, b, inst->operand(0u), llvm::Intrinsic::log2);
            case xir::IntrinsicOp::LOG10: return _translate_unary_fp_math_operation(current, b, inst->operand(0u), llvm::Intrinsic::log10);
            case xir::IntrinsicOp::POW: return _translate_binary_fp_math_operation(current, b, inst->operand(0u), inst->operand(1u), llvm::Intrinsic::pow);
            case xir::IntrinsicOp::POW_INT: {
                auto base = inst->operand(0u);
                auto index = inst->operand(1u);
                auto llvm_base = _lookup_value(current, b, base);
                auto llvm_index = _lookup_value(current, b, index);
                LUISA_ASSERT(llvm_base->getType()->isFPOrFPVectorTy() &&
                                 llvm_index->getType()->isIntOrIntVectorTy() &&
                                 !llvm_index->getType()->isIntOrIntVectorTy(1),
                             "Invalid operand type for pow_int operation.");
                if (llvm_base->getType()->isVectorTy() || llvm_index->getType()->isVectorTy()) {
                    LUISA_ASSERT(llvm_base->getType()->isVectorTy() && llvm_index->getType()->isVectorTy() &&
                                     llvm::cast<llvm::VectorType>(llvm_base->getType())->getElementCount() ==
                                         llvm::cast<llvm::VectorType>(llvm_index->getType())->getElementCount(),
                                 "pow_int operation requires both operands to be vectors of the same dimension.");
                }
                return b.CreateBinaryIntrinsic(llvm::Intrinsic::powi, llvm_base, llvm_index);
            }
            case xir::IntrinsicOp::SQRT: return _translate_unary_fp_math_operation(current, b, inst->operand(0u), llvm::Intrinsic::sqrt);
            case xir::IntrinsicOp::RSQRT: {
                auto llvm_operand = _lookup_value(current, b, inst->operand(0u));
                auto llvm_sqrt = b.CreateUnaryIntrinsic(llvm::Intrinsic::sqrt, llvm_operand);
                auto operand_type = inst->operand(0u)->type();
                auto operand_elem_type = operand_type->is_vector() ? operand_type->element() : operand_type;
                auto llvm_elem_type = _translate_type(operand_elem_type, false);
                auto llvm_one = llvm::ConstantFP::get(llvm_elem_type, 1.f);
                if (operand_type->is_vector()) {
                    auto dim = operand_type->dimension();
                    llvm_one = llvm::ConstantVector::getSplat(llvm::ElementCount::getFixed(dim), llvm_one);
                }
                return b.CreateFDiv(llvm_one, llvm_sqrt);
            }
            case xir::IntrinsicOp::CEIL: return _translate_unary_fp_math_operation(current, b, inst->operand(0u), llvm::Intrinsic::ceil);
            case xir::IntrinsicOp::FLOOR: return _translate_unary_fp_math_operation(current, b, inst->operand(0u), llvm::Intrinsic::floor);
            case xir::IntrinsicOp::FRACT: {
                // fract(x) = x - floor(x)
                auto llvm_operand = _lookup_value(current, b, inst->operand(0u));
                auto llvm_floor = _translate_unary_fp_math_operation(current, b, inst->operand(0u), llvm::Intrinsic::floor);
                return b.CreateFSub(llvm_operand, llvm_floor);
            }
            case xir::IntrinsicOp::TRUNC: return _translate_unary_fp_math_operation(current, b, inst->operand(0u), llvm::Intrinsic::trunc);
            case xir::IntrinsicOp::ROUND: return _translate_unary_fp_math_operation(current, b, inst->operand(0u), llvm::Intrinsic::round);
            case xir::IntrinsicOp::RINT: return _translate_unary_fp_math_operation(current, b, inst->operand(0u), llvm::Intrinsic::rint);
            case xir::IntrinsicOp::FMA: {
                // fma(a, b, c) = a * b + c, or we can use llvm intrinsic for fp
                auto va = inst->operand(0u);
                auto vb = inst->operand(1u);
                auto vc = inst->operand(2u);
                LUISA_ASSERT(va->type() != nullptr && va->type() == vb->type() && va->type() == vc->type(), "Type mismatch for fma.");
                auto llvm_va = _lookup_value(current, b, va);
                auto llvm_vb = _lookup_value(current, b, vb);
                auto llvm_vc = _lookup_value(current, b, vc);
                auto elem_type = va->type()->is_vector() ? va->type()->element() : va->type();
                switch (elem_type->tag()) {
                    case Type::Tag::INT8: [[fallthrough]];
                    case Type::Tag::INT16: [[fallthrough]];
                    case Type::Tag::INT32: [[fallthrough]];
                    case Type::Tag::INT64: {
                        auto llvm_mul = b.CreateNSWMul(llvm_va, llvm_vb);
                        return b.CreateNSWAdd(llvm_mul, llvm_vc);
                    }
                    case Type::Tag::UINT8: [[fallthrough]];
                    case Type::Tag::UINT16: [[fallthrough]];
                    case Type::Tag::UINT32: [[fallthrough]];
                    case Type::Tag::UINT64: {
                        auto llvm_mul = b.CreateMul(llvm_va, llvm_vb);
                        return b.CreateAdd(llvm_mul, llvm_vc);
                    }
                    case Type::Tag::FLOAT16: [[fallthrough]];
                    case Type::Tag::FLOAT32: [[fallthrough]];
                    case Type::Tag::FLOAT64: {
                        return b.CreateIntrinsic(llvm_va->getType(), llvm::Intrinsic::fma, {llvm_va, llvm_vb, llvm_vc});
                    }
                    default: break;
                }
                LUISA_ERROR_WITH_LOCATION("Invalid operand type for fma operation: {}.", va->type()->description());
            }
            case xir::IntrinsicOp::COPYSIGN: return _translate_binary_fp_math_operation(current, b, inst->operand(0u), inst->operand(1u), llvm::Intrinsic::copysign);
            case xir::IntrinsicOp::CROSS: {
                // cross(u, v) = (u.y * v.z - v.y * u.z, u.z * v.x - v.z * u.x, u.x * v.y - v.x * u.y)
                auto u = _lookup_value(current, b, inst->operand(0u));
                auto v = _lookup_value(current, b, inst->operand(1u));
                auto poison = llvm::PoisonValue::get(u->getType());
                auto s0 = b.CreateShuffleVector(u, poison, {1, 2, 0});
                auto s1 = b.CreateShuffleVector(v, poison, {2, 0, 1});
                auto s2 = b.CreateShuffleVector(v, poison, {1, 2, 0});
                auto s3 = b.CreateShuffleVector(u, poison, {2, 0, 1});
                auto s01 = b.CreateFMul(s0, s1);
                auto s23 = b.CreateFMul(s2, s3);
                return b.CreateFSub(s01, s23);
            }
            case xir::IntrinsicOp::DOT: return _translate_vector_dot(current, b, inst->operand(0u), inst->operand(1u));
            case xir::IntrinsicOp::LENGTH: {
                auto v = inst->operand(0u);
                auto llvm_length_squared = _translate_vector_dot(current, b, v, v);
                return b.CreateUnaryIntrinsic(llvm::Intrinsic::sqrt, llvm_length_squared);
            }
            case xir::IntrinsicOp::LENGTH_SQUARED: {
                auto v = inst->operand(0u);
                return _translate_vector_dot(current, b, v, v);
            }
            case xir::IntrinsicOp::NORMALIZE: {
                auto v = inst->operand(0u);
                auto llvm_length_squared = _translate_vector_dot(current, b, v, v);
                auto llvm_length = b.CreateUnaryIntrinsic(llvm::Intrinsic::sqrt, llvm_length_squared);
                auto llvm_v = _lookup_value(current, b, v);
                auto llvm_length_splat = b.CreateVectorSplat(v->type()->dimension(), llvm_length);
                return b.CreateFDiv(llvm_v, llvm_length_splat);
            }
            case xir::IntrinsicOp::FACEFORWARD: {
                // faceforward(n, i, n_ref) = dot(i, n_ref) < 0 ? n : -n
                auto n = inst->operand(0u);
                auto i = inst->operand(1u);
                auto n_ref = inst->operand(2u);
                auto llvm_dot = _translate_vector_dot(current, b, i, n_ref);
                auto llvm_zero = llvm::ConstantFP::get(llvm_dot->getType(), 0.);
                auto llvm_cmp = b.CreateFCmpOLT(llvm_dot, llvm_zero);
                auto llvm_pos_n = _lookup_value(current, b, n);
                auto llvm_neg_n = b.CreateFNeg(llvm_pos_n);
                return b.CreateSelect(llvm_cmp, llvm_pos_n, llvm_neg_n);
            }
            case xir::IntrinsicOp::REFLECT: {
                // reflect(I, N) = I - 2.0 * dot(N, I) * N = fma(splat(-2.0 * dot(N, I)), N, I)
                auto i = inst->operand(0u);
                auto n = inst->operand(1u);
                auto llvm_dot = _translate_vector_dot(current, b, n, i);
                auto llvm_i = _lookup_value(current, b, i);
                auto llvm_n = _lookup_value(current, b, n);
                auto dim = llvm::cast<llvm::VectorType>(llvm_n->getType())->getElementCount();
                auto llvm_minus_two = llvm::ConstantFP::get(llvm_dot->getType(), -2.);
                auto llvm_minus_two_dot = b.CreateFMul(llvm_minus_two, llvm_dot);
                llvm_minus_two_dot = b.CreateVectorSplat(dim, llvm_minus_two_dot);
                return b.CreateIntrinsic(llvm_n->getType(), llvm::Intrinsic::fma,
                                         {llvm_minus_two_dot, llvm_n, llvm_i});
            }
            case xir::IntrinsicOp::REDUCE_SUM: return _translate_vector_reduce(current, b, xir::IntrinsicOp::REDUCE_SUM, inst->operand(0u));
            case xir::IntrinsicOp::REDUCE_PRODUCT: return _translate_vector_reduce(current, b, xir::IntrinsicOp::REDUCE_PRODUCT, inst->operand(0u));
            case xir::IntrinsicOp::REDUCE_MIN: return _translate_vector_reduce(current, b, xir::IntrinsicOp::REDUCE_MIN, inst->operand(0u));
            case xir::IntrinsicOp::REDUCE_MAX: return _translate_vector_reduce(current, b, xir::IntrinsicOp::REDUCE_MAX, inst->operand(0u));
            case xir::IntrinsicOp::OUTER_PRODUCT: break;
            case xir::IntrinsicOp::MATRIX_COMP_NEG: break;
            case xir::IntrinsicOp::MATRIX_COMP_ADD: break;
            case xir::IntrinsicOp::MATRIX_COMP_SUB: break;
            case xir::IntrinsicOp::MATRIX_COMP_MUL: break;
            case xir::IntrinsicOp::MATRIX_COMP_DIV: break;
            case xir::IntrinsicOp::MATRIX_LINALG_MUL: break;
            case xir::IntrinsicOp::MATRIX_DETERMINANT: break;
            case xir::IntrinsicOp::MATRIX_TRANSPOSE: break;
            case xir::IntrinsicOp::MATRIX_INVERSE: break;
            case xir::IntrinsicOp::ATOMIC_EXCHANGE: break;
            case xir::IntrinsicOp::ATOMIC_COMPARE_EXCHANGE: break;
            case xir::IntrinsicOp::ATOMIC_FETCH_ADD: break;
            case xir::IntrinsicOp::ATOMIC_FETCH_SUB: break;
            case xir::IntrinsicOp::ATOMIC_FETCH_AND: break;
            case xir::IntrinsicOp::ATOMIC_FETCH_OR: break;
            case xir::IntrinsicOp::ATOMIC_FETCH_XOR: break;
            case xir::IntrinsicOp::ATOMIC_FETCH_MIN: break;
            case xir::IntrinsicOp::ATOMIC_FETCH_MAX: break;
            case xir::IntrinsicOp::BUFFER_READ: return _translate_buffer_read(current, b, inst);
            case xir::IntrinsicOp::BUFFER_WRITE: return _translate_buffer_write(current, b, inst);
            case xir::IntrinsicOp::BUFFER_SIZE: break;
            case xir::IntrinsicOp::BYTE_BUFFER_READ: break;
            case xir::IntrinsicOp::BYTE_BUFFER_WRITE: break;
            case xir::IntrinsicOp::BYTE_BUFFER_SIZE: break;
            case xir::IntrinsicOp::TEXTURE2D_READ: break;
            case xir::IntrinsicOp::TEXTURE2D_WRITE: break;
            case xir::IntrinsicOp::TEXTURE2D_SIZE: break;
            case xir::IntrinsicOp::TEXTURE2D_SAMPLE: break;
            case xir::IntrinsicOp::TEXTURE2D_SAMPLE_LEVEL: break;
            case xir::IntrinsicOp::TEXTURE2D_SAMPLE_GRAD: break;
            case xir::IntrinsicOp::TEXTURE2D_SAMPLE_GRAD_LEVEL: break;
            case xir::IntrinsicOp::TEXTURE3D_READ: break;
            case xir::IntrinsicOp::TEXTURE3D_WRITE: break;
            case xir::IntrinsicOp::TEXTURE3D_SIZE: break;
            case xir::IntrinsicOp::TEXTURE3D_SAMPLE: break;
            case xir::IntrinsicOp::TEXTURE3D_SAMPLE_LEVEL: break;
            case xir::IntrinsicOp::TEXTURE3D_SAMPLE_GRAD: break;
            case xir::IntrinsicOp::TEXTURE3D_SAMPLE_GRAD_LEVEL: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE2D_SAMPLE: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE2D_SAMPLE_LEVEL: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE2D_SAMPLE_GRAD: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE2D_SAMPLE_GRAD_LEVEL: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE3D_SAMPLE: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE3D_SAMPLE_LEVEL: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE3D_SAMPLE_GRAD: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE3D_SAMPLE_GRAD_LEVEL: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE2D_SAMPLE_SAMPLER: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE2D_SAMPLE_LEVEL_SAMPLER: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE2D_SAMPLE_GRAD_SAMPLER: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE2D_SAMPLE_GRAD_LEVEL_SAMPLER: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE3D_SAMPLE_SAMPLER: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE3D_SAMPLE_LEVEL_SAMPLER: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE3D_SAMPLE_GRAD_SAMPLER: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE3D_SAMPLE_GRAD_LEVEL_SAMPLER: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE2D_READ: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE3D_READ: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE2D_READ_LEVEL: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE3D_READ_LEVEL: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE2D_SIZE: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE3D_SIZE: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE2D_SIZE_LEVEL: break;
            case xir::IntrinsicOp::BINDLESS_TEXTURE3D_SIZE_LEVEL: break;
            case xir::IntrinsicOp::BINDLESS_BUFFER_READ: break;
            case xir::IntrinsicOp::BINDLESS_BUFFER_WRITE: break;
            case xir::IntrinsicOp::BINDLESS_BUFFER_SIZE: break;
            case xir::IntrinsicOp::BINDLESS_BUFFER_TYPE: break;
            case xir::IntrinsicOp::BINDLESS_BYTE_BUFFER_READ: break;
            case xir::IntrinsicOp::BINDLESS_BYTE_BUFFER_WRITE: break;
            case xir::IntrinsicOp::BINDLESS_BYTE_BUFFER_SIZE: break;
            case xir::IntrinsicOp::BUFFER_DEVICE_ADDRESS: break;
            case xir::IntrinsicOp::BINDLESS_BUFFER_DEVICE_ADDRESS: break;
            case xir::IntrinsicOp::DEVICE_ADDRESS_READ: break;
            case xir::IntrinsicOp::DEVICE_ADDRESS_WRITE: break;
            case xir::IntrinsicOp::AGGREGATE: break;
            case xir::IntrinsicOp::SHUFFLE: break;
            case xir::IntrinsicOp::INSERT: return _translate_insert(current, b, inst);
            case xir::IntrinsicOp::EXTRACT: return _translate_extract(current, b, inst);
            case xir::IntrinsicOp::AUTODIFF_REQUIRES_GRADIENT: break;
            case xir::IntrinsicOp::AUTODIFF_GRADIENT: break;
            case xir::IntrinsicOp::AUTODIFF_GRADIENT_MARKER: break;
            case xir::IntrinsicOp::AUTODIFF_ACCUMULATE_GRADIENT: break;
            case xir::IntrinsicOp::AUTODIFF_BACKWARD: break;
            case xir::IntrinsicOp::AUTODIFF_DETACH: break;
            case xir::IntrinsicOp::RAY_TRACING_INSTANCE_TRANSFORM: break;
            case xir::IntrinsicOp::RAY_TRACING_INSTANCE_USER_ID: break;
            case xir::IntrinsicOp::RAY_TRACING_INSTANCE_VISIBILITY_MASK: break;
            case xir::IntrinsicOp::RAY_TRACING_SET_INSTANCE_TRANSFORM: break;
            case xir::IntrinsicOp::RAY_TRACING_SET_INSTANCE_VISIBILITY: break;
            case xir::IntrinsicOp::RAY_TRACING_SET_INSTANCE_OPACITY: break;
            case xir::IntrinsicOp::RAY_TRACING_SET_INSTANCE_USER_ID: break;
            case xir::IntrinsicOp::RAY_TRACING_TRACE_CLOSEST: break;
            case xir::IntrinsicOp::RAY_TRACING_TRACE_ANY: break;
            case xir::IntrinsicOp::RAY_TRACING_QUERY_ALL: break;
            case xir::IntrinsicOp::RAY_TRACING_QUERY_ANY: break;
            case xir::IntrinsicOp::RAY_TRACING_INSTANCE_MOTION_MATRIX: break;
            case xir::IntrinsicOp::RAY_TRACING_INSTANCE_MOTION_SRT: break;
            case xir::IntrinsicOp::RAY_TRACING_SET_INSTANCE_MOTION_MATRIX: break;
            case xir::IntrinsicOp::RAY_TRACING_SET_INSTANCE_MOTION_SRT: break;
            case xir::IntrinsicOp::RAY_TRACING_TRACE_CLOSEST_MOTION_BLUR: break;
            case xir::IntrinsicOp::RAY_TRACING_TRACE_ANY_MOTION_BLUR: break;
            case xir::IntrinsicOp::RAY_TRACING_QUERY_ALL_MOTION_BLUR: break;
            case xir::IntrinsicOp::RAY_TRACING_QUERY_ANY_MOTION_BLUR: break;
            case xir::IntrinsicOp::RAY_QUERY_WORLD_SPACE_RAY: break;
            case xir::IntrinsicOp::RAY_QUERY_PROCEDURAL_CANDIDATE_HIT: break;
            case xir::IntrinsicOp::RAY_QUERY_TRIANGLE_CANDIDATE_HIT: break;
            case xir::IntrinsicOp::RAY_QUERY_COMMITTED_HIT: break;
            case xir::IntrinsicOp::RAY_QUERY_COMMIT_TRIANGLE: break;
            case xir::IntrinsicOp::RAY_QUERY_COMMIT_PROCEDURAL: break;
            case xir::IntrinsicOp::RAY_QUERY_TERMINATE: break;
            case xir::IntrinsicOp::RAY_QUERY_PROCEED: break;
            case xir::IntrinsicOp::RAY_QUERY_IS_TRIANGLE_CANDIDATE: break;
            case xir::IntrinsicOp::RAY_QUERY_IS_PROCEDURAL_CANDIDATE: break;
            case xir::IntrinsicOp::RASTER_DISCARD: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::RASTER_DDX: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::RASTER_DDY: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_IS_FIRST_ACTIVE_LANE: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_FIRST_ACTIVE_LANE: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_ACTIVE_ALL_EQUAL: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_ACTIVE_BIT_AND: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_ACTIVE_BIT_OR: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_ACTIVE_BIT_XOR: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_ACTIVE_COUNT_BITS: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_ACTIVE_MAX: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_ACTIVE_MIN: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_ACTIVE_PRODUCT: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_ACTIVE_SUM: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_ACTIVE_ALL: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_ACTIVE_ANY: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_ACTIVE_BIT_MASK: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_PREFIX_COUNT_BITS: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_PREFIX_SUM: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_PREFIX_PRODUCT: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_READ_LANE: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::WARP_READ_FIRST_ACTIVE_LANE: LUISA_NOT_IMPLEMENTED();
            case xir::IntrinsicOp::INDIRECT_DISPATCH_SET_KERNEL: break;
            case xir::IntrinsicOp::INDIRECT_DISPATCH_SET_COUNT: break;
            case xir::IntrinsicOp::SHADER_EXECUTION_REORDER: return nullptr;// no-op on the LLVM side
        }
        // TODO: implement
        if (auto llvm_ret_type = _translate_type(inst->type(), true)) {
            return llvm::Constant::getNullValue(llvm_ret_type);
        }
        return nullptr;
        LUISA_NOT_IMPLEMENTED();
    }

    [[nodiscard]] llvm::Value *_translate_cast_inst(CurrentFunction &current, IRBuilder &b,
                                                    const Type *dst_type, xir::CastOp op,
                                                    const xir::Value *src_value) noexcept {
        auto llvm_value = _lookup_value(current, b, src_value);
        auto src_type = src_value->type();
        switch (op) {
            case xir::CastOp::STATIC_CAST: {
                if (dst_type == src_type) { return llvm_value; }
                LUISA_ASSERT((dst_type->is_scalar() && src_type->is_scalar()) ||
                                 (dst_type->is_vector() && src_type->is_vector() &&
                                  dst_type->dimension() == src_type->dimension()),
                             "Invalid static cast.");
                auto dst_is_scalar = dst_type->is_scalar();
                auto src_is_scalar = src_type->is_scalar();
                auto dst_elem_type = dst_is_scalar ? dst_type : dst_type->element();
                auto src_elem_type = src_is_scalar ? src_type : src_type->element();
                // typeN -> boolN
                if (dst_elem_type->is_bool()) {
                    auto cmp = _cmp_ne_zero(b, llvm_value);
                    return _zext_i1_to_i8(b, cmp);
                }
                // general case
                auto classify = [](const Type *t) noexcept {
                    return std::make_tuple(
                        t->is_float16() || t->is_float32() || t->is_float64(),
                        t->is_int8() || t->is_int16() || t->is_int32() || t->is_int64(),
                        t->is_uint8() || t->is_uint16() || t->is_uint32() || t->is_uint64() || t->is_bool());
                };
                auto llvm_dst_type = _translate_type(dst_type, true);
                auto [dst_float, dst_int, dst_uint] = classify(dst_elem_type);
                auto [src_float, src_int, src_uint] = classify(src_elem_type);
                if (dst_float) {
                    if (src_float) { return b.CreateFPCast(llvm_value, llvm_dst_type); }
                    if (src_int) { return b.CreateSIToFP(llvm_value, llvm_dst_type); }
                    if (src_uint) { return b.CreateUIToFP(llvm_value, llvm_dst_type); }
                }
                if (dst_int) {
                    if (src_float) { return b.CreateFPToSI(llvm_value, llvm_dst_type); }
                    if (src_int) { return b.CreateIntCast(llvm_value, llvm_dst_type, true); }
                    if (src_uint) { return b.CreateIntCast(llvm_value, llvm_dst_type, false); }
                }
                if (dst_uint) {
                    if (src_float) { return b.CreateFPToUI(llvm_value, llvm_dst_type); }
                    if (src_int) { return b.CreateIntCast(llvm_value, llvm_dst_type, true); }
                    if (src_uint) { return b.CreateIntCast(llvm_value, llvm_dst_type, false); }
                }
                LUISA_ERROR_WITH_LOCATION("Invalid static cast.");
            }
            case xir::CastOp::BITWISE_CAST: {
                // create a temporary alloca
                auto alignment = std::max(src_type->alignment(), dst_type->alignment());
                LUISA_ASSERT(src_type->size() == dst_type->size(), "Bitwise cast size mismatch.");
                auto llvm_src_type = _translate_type(src_type, true);
                auto llvm_dst_type = _translate_type(dst_type, true);
                auto llvm_temp = b.CreateAlloca(llvm_src_type);
                if (llvm_temp->getAlign() < alignment) {
                    llvm_temp->setAlignment(llvm::Align{alignment});
                }
                // store src value
                b.CreateAlignedStore(llvm_value, llvm_temp, llvm::MaybeAlign{alignment});
                // load dst value
                return b.CreateAlignedLoad(llvm_dst_type, llvm_temp, llvm::MaybeAlign{alignment});
            }
        }
        LUISA_ERROR_WITH_LOCATION("Invalid cast operation.");
    }

    [[nodiscard]] llvm::Value *_translate_instruction(CurrentFunction &current, IRBuilder &b, const xir::Instruction *inst) noexcept {
        switch (inst->derived_instruction_tag()) {
            case xir::DerivedInstructionTag::SENTINEL: {
                LUISA_ERROR_WITH_LOCATION("Invalid instruction.");
            }
            case xir::DerivedInstructionTag::IF: {
                auto if_inst = static_cast<const xir::IfInst *>(inst);
                auto llvm_condition = _lookup_value(current, b, if_inst->condition());
                auto llvm_false = b.getInt8(0);
                llvm_condition = b.CreateICmpNE(llvm_condition, llvm_false);
                auto llvm_true_block = _find_or_create_basic_block(current, if_inst->true_block());
                auto llvm_false_block = _find_or_create_basic_block(current, if_inst->false_block());
                auto llvm_merge_block = _find_or_create_basic_block(current, if_inst->merge_block());
                auto llvm_inst = b.CreateCondBr(llvm_condition, llvm_true_block, llvm_false_block);
                _translate_instructions_in_basic_block(current, llvm_true_block, if_inst->true_block());
                _translate_instructions_in_basic_block(current, llvm_false_block, if_inst->false_block());
                _translate_instructions_in_basic_block(current, llvm_merge_block, if_inst->merge_block());
                return llvm_inst;
            }
            case xir::DerivedInstructionTag::SWITCH: {
                auto switch_inst = static_cast<const xir::SwitchInst *>(inst);
                auto llvm_condition = _lookup_value(current, b, switch_inst->value());
                auto llvm_merge_block = _find_or_create_basic_block(current, switch_inst->merge_block());
                auto llvm_default_block = _find_or_create_basic_block(current, switch_inst->default_block());
                auto llvm_inst = b.CreateSwitch(llvm_condition, llvm_default_block, switch_inst->case_count());
                for (auto i = 0u; i < switch_inst->case_count(); i++) {
                    auto case_value = switch_inst->case_value(i);
                    auto llvm_case_value = b.getInt32(case_value);
                    auto llvm_case_block = _find_or_create_basic_block(current, switch_inst->case_block(i));
                    llvm_inst->addCase(llvm_case_value, llvm_case_block);
                }
                _translate_instructions_in_basic_block(current, llvm_default_block, switch_inst->default_block());
                for (auto &case_block_use : switch_inst->case_block_uses()) {
                    auto case_block = static_cast<const xir::BasicBlock *>(case_block_use->value());
                    auto llvm_case_block = _find_or_create_basic_block(current, case_block);
                    _translate_instructions_in_basic_block(current, llvm_case_block, case_block);
                }
                _translate_instructions_in_basic_block(current, llvm_merge_block, switch_inst->merge_block());
                return llvm_inst;
            }
            case xir::DerivedInstructionTag::LOOP: {
                auto loop_inst = static_cast<const xir::LoopInst *>(inst);
                auto llvm_merge_block = _find_or_create_basic_block(current, loop_inst->merge_block());
                auto llvm_prepare_block = _find_or_create_basic_block(current, loop_inst->prepare_block());
                auto llvm_body_block = _find_or_create_basic_block(current, loop_inst->body_block());
                auto llvm_update_block = _find_or_create_basic_block(current, loop_inst->update_block());
                auto llvm_inst = b.CreateBr(llvm_prepare_block);
                _translate_instructions_in_basic_block(current, llvm_prepare_block, loop_inst->prepare_block());
                _translate_instructions_in_basic_block(current, llvm_body_block, loop_inst->body_block());
                _translate_instructions_in_basic_block(current, llvm_update_block, loop_inst->update_block());
                _translate_instructions_in_basic_block(current, llvm_merge_block, loop_inst->merge_block());
                return llvm_inst;
            }
            case xir::DerivedInstructionTag::SIMPLE_LOOP: {
                auto loop_inst = static_cast<const xir::SimpleLoopInst *>(inst);
                auto llvm_merge_block = _find_or_create_basic_block(current, loop_inst->merge_block());
                auto llvm_body_block = _find_or_create_basic_block(current, loop_inst->body_block());
                auto llvm_inst = b.CreateBr(llvm_body_block);
                _translate_instructions_in_basic_block(current, llvm_body_block, loop_inst->body_block());
                _translate_instructions_in_basic_block(current, llvm_merge_block, loop_inst->merge_block());
                return llvm_inst;
            }
            case xir::DerivedInstructionTag::CONDITIONAL_BRANCH: {
                auto cond_br_inst = static_cast<const xir::ConditionalBranchInst *>(inst);
                auto llvm_condition = _lookup_value(current, b, cond_br_inst->condition());
                auto llvm_false = b.getInt8(0);
                llvm_condition = b.CreateICmpNE(llvm_condition, llvm_false);
                auto llvm_true_block = _find_or_create_basic_block(current, cond_br_inst->true_block());
                auto llvm_false_block = _find_or_create_basic_block(current, cond_br_inst->false_block());
                return b.CreateCondBr(llvm_condition, llvm_true_block, llvm_false_block);
            }
            case xir::DerivedInstructionTag::UNREACHABLE: {
                LUISA_ASSERT(inst->type() == nullptr, "Unreachable instruction should not have a type.");
                return b.CreateUnreachable();
            }
            case xir::DerivedInstructionTag::BRANCH: [[fallthrough]];
            case xir::DerivedInstructionTag::BREAK: [[fallthrough]];
            case xir::DerivedInstructionTag::CONTINUE: {
                auto br_inst = static_cast<const xir::BranchTerminatorInstruction *>(inst);
                auto llvm_target_block = _find_or_create_basic_block(current, br_inst->target_block());
                return b.CreateBr(llvm_target_block);
            }
            case xir::DerivedInstructionTag::RETURN: {
                auto return_inst = static_cast<const xir::ReturnInst *>(inst);
                if (auto ret_val = return_inst->return_value()) {
                    auto llvm_ret_val = _lookup_value(current, b, ret_val);
                    return b.CreateRet(llvm_ret_val);
                }
                return b.CreateRetVoid();
            }
            case xir::DerivedInstructionTag::PHI: {
                LUISA_ERROR_WITH_LOCATION("Unexpected phi instruction. Please run reg2mem pass first.");
            }
            case xir::DerivedInstructionTag::ALLOCA: {
                auto llvm_type = _translate_type(inst->type(), false);
                auto llvm_inst = b.CreateAlloca(llvm_type);
                auto alignment = inst->type()->alignment();
                if (llvm_inst->getAlign() < alignment) {
                    llvm_inst->setAlignment(llvm::Align{alignment});
                }
                return llvm_inst;
            }
            case xir::DerivedInstructionTag::LOAD: {
                auto load_inst = static_cast<const xir::LoadInst *>(inst);
                auto alignment = load_inst->variable()->type()->alignment();
                auto llvm_type = _translate_type(load_inst->type(), true);
                auto llvm_ptr = _lookup_value(current, b, load_inst->variable());
                return b.CreateAlignedLoad(llvm_type, llvm_ptr, llvm::MaybeAlign{alignment});
            }
            case xir::DerivedInstructionTag::STORE: {
                auto store_inst = static_cast<const xir::StoreInst *>(inst);
                auto alignment = store_inst->variable()->type()->alignment();
                auto llvm_ptr = _lookup_value(current, b, store_inst->variable());
                auto llvm_value = _lookup_value(current, b, store_inst->value());
                return b.CreateAlignedStore(llvm_value, llvm_ptr, llvm::MaybeAlign{alignment});
            }
            case xir::DerivedInstructionTag::GEP: {
                auto gep_inst = static_cast<const xir::GEPInst *>(inst);
                auto ptr = gep_inst->base();
                auto llvm_ptr = _lookup_value(current, b, ptr, false);
                return _translate_gep(current, b, gep_inst->type(), ptr->type(), llvm_ptr, gep_inst->index_uses());
            }
            case xir::DerivedInstructionTag::CALL: {
                auto call_inst = static_cast<const xir::CallInst *>(inst);
                auto llvm_func = llvm::cast<llvm::Function>(_lookup_value(current, b, call_inst->callee()));
                llvm::SmallVector<llvm::Value *, 16u> llvm_args;
                llvm_args.reserve(call_inst->argument_count());
                for (auto i = 0u; i < call_inst->argument_count(); i++) {
                    auto llvm_arg = _lookup_value(current, b, call_inst->argument(i));
                    llvm_args.emplace_back(llvm_arg);
                }
                // built-in variables
                for (auto &builtin_variable : current.builtin_variables) {
                    llvm_args.emplace_back(builtin_variable);
                }
                return b.CreateCall(llvm_func, llvm_args);
            }
            case xir::DerivedInstructionTag::INTRINSIC: {
                auto intrinsic_inst = static_cast<const xir::IntrinsicInst *>(inst);
                return _translate_intrinsic_inst(current, b, intrinsic_inst);
            }
            case xir::DerivedInstructionTag::CAST: {
                auto cast_inst = static_cast<const xir::CastInst *>(inst);
                return _translate_cast_inst(current, b, cast_inst->type(), cast_inst->op(), cast_inst->value());
            }
            case xir::DerivedInstructionTag::PRINT: {
                LUISA_WARNING_WITH_LOCATION("Ignoring print instruction.");// TODO...
                return nullptr;
            }
            case xir::DerivedInstructionTag::ASSERT: {
                auto assert_inst = static_cast<const xir::AssertInst *>(inst);
                auto llvm_condition = _lookup_value(current, b, assert_inst->condition());
                auto llvm_message = _translate_string_or_null(b, assert_inst->message());
                auto llvm_void_type = llvm::Type::getVoidTy(_llvm_context);
                auto assert_func_type = llvm::FunctionType::get(llvm_void_type, {llvm_condition->getType(), llvm_message->getType()}, false);
                auto external_assert = _llvm_module->getOrInsertFunction("luisa.assert", assert_func_type);
                auto external_assert_signature = llvm::cast<llvm::Function>(external_assert.getCallee());
                external_assert_signature->addFnAttr(llvm::Attribute::NoCapture);
                external_assert_signature->addFnAttr(llvm::Attribute::ReadOnly);
                return b.CreateCall(external_assert, {llvm_condition, llvm_message});
            }
            case xir::DerivedInstructionTag::ASSUME: {
                auto assume_inst = static_cast<const xir::AssumeInst *>(inst);
                auto llvm_condition = _lookup_value(current, b, assume_inst->condition());
                // TODO: we ignore assumption message for now
                return b.CreateAssumption(llvm_condition);
            }
            case xir::DerivedInstructionTag::OUTLINE: {
                auto outline_inst = static_cast<const xir::OutlineInst *>(inst);
                auto llvm_target_block = _find_or_create_basic_block(current, outline_inst->target_block());
                auto llvm_merge_block = _find_or_create_basic_block(current, outline_inst->merge_block());
                auto llvm_inst = b.CreateBr(llvm_target_block);
                _translate_instructions_in_basic_block(current, llvm_target_block, outline_inst->target_block());
                _translate_instructions_in_basic_block(current, llvm_merge_block, outline_inst->merge_block());
                return llvm_inst;
            }
            case xir::DerivedInstructionTag::AUTO_DIFF: LUISA_NOT_IMPLEMENTED();
            case xir::DerivedInstructionTag::RAY_QUERY: LUISA_NOT_IMPLEMENTED();
        }
        LUISA_ERROR_WITH_LOCATION("Invalid instruction.");
    }

    [[nodiscard]] llvm::BasicBlock *_find_or_create_basic_block(CurrentFunction &current, const xir::BasicBlock *bb) noexcept {
        auto iter = current.value_map.try_emplace(bb, nullptr).first;
        if (iter->second) { return llvm::cast<llvm::BasicBlock>(iter->second); }
        auto llvm_bb = llvm::BasicBlock::Create(_llvm_context, _get_name_from_metadata(bb), current.func);
        iter->second = llvm_bb;
        return llvm_bb;
    }

    void _translate_instructions_in_basic_block(CurrentFunction &current, llvm::BasicBlock *llvm_bb, const xir::BasicBlock *bb) noexcept {
        if (current.translated_basic_blocks.emplace(llvm_bb).second) {
            for (auto &inst : bb->instructions()) {
                IRBuilder b{llvm_bb};
                auto llvm_value = _translate_instruction(current, b, &inst);
                auto [_, success] = current.value_map.emplace(&inst, llvm_value);
                LUISA_ASSERT(success, "Instruction already translated.");
            }
        }
    }

    [[nodiscard]] llvm::BasicBlock *_translate_basic_block(CurrentFunction &current, const xir::BasicBlock *bb) noexcept {
        auto llvm_bb = _find_or_create_basic_block(current, bb);
        _translate_instructions_in_basic_block(current, llvm_bb, bb);
        return llvm_bb;
    }

    [[nodiscard]] llvm::Function *_translate_kernel_function(const xir::KernelFunction *f) noexcept {
        // create a wrapper function for the kernel with the following template:
        // struct Params { params... };
        // struct LaunchConfig {
        //   uint3 block_id;
        //   uint3 dispatch_size;
        //   uint3 block_size;
        // };
        // void kernel_wrapper(Params *params, LaunchConfig *config) {
        // entry:
        //   block_id = config->block_id;
        //   block_size = config->block_size;
        //   /* assume(block_size == f.block_size) */
        //   dispatch_size = config->dispatch_size;
        //   thread_count = block_size.x * block_size.y * block_size.z;
        //   pi = alloca i32;
        //   store 0, pi;
        //   br loop;
        // loop:
        //   i = load pi;
        //   thread_id_x = i % block_size.x;
        //   thread_id_y = (i / block_size.x) % block_size.y;
        //   thread_id_z = i / (block_size.x * block_size.y);
        //   thread_id = (thread_id_x, thread_id_y, thread_id_z);
        //   dispatch_id = block_id * block_size + thread_id;
        //   in_range = reduce_and(dispatch_id < dispatch_size);
        //   br in_range, body, update;
        // body:
        //   call f(params, thread_id, block_id, dispatch_id, block_size, dispatch_size);
        //   br update;
        // update:
        //   next_i = i + 1;
        //   store next_i, pi;
        //   br next_i < thread_count, loop, merge;
        // merge:
        //   ret;

        // create the kernel function
        auto llvm_kernel = _translate_function_definition(f, llvm::Function::PrivateLinkage, "kernel");

        // create the wrapper function
        auto llvm_void_type = llvm::Type::getVoidTy(_llvm_context);
        auto llvm_ptr_type = llvm::PointerType::get(_llvm_context, 0);
        auto function_name = luisa::format("kernel.main", luisa::string_view{llvm_kernel->getName()});
        auto llvm_wrapper_type = llvm::FunctionType::get(llvm_void_type, {llvm_ptr_type, llvm_ptr_type}, false);
        auto llvm_wrapper_function = llvm::Function::Create(llvm_wrapper_type, llvm::Function::ExternalLinkage, llvm::Twine{function_name}, _llvm_module);
        llvm_wrapper_function->getArg(0)->setName("launch.params");
        llvm_wrapper_function->getArg(1)->setName("launch.config");
        _llvm_functions.emplace(f, llvm_wrapper_function);

        // create the entry block for the wrapper function
        auto llvm_entry_block = llvm::BasicBlock::Create(_llvm_context, "entry", llvm_wrapper_function);
        IRBuilder b{llvm_entry_block};

        auto arg_count = f->arguments().size();

        // create the params struct type
        auto llvm_i8_type = llvm::Type::getInt8Ty(_llvm_context);
        llvm::SmallVector<size_t, 32u> padded_param_indices;
        llvm::SmallVector<llvm::Type *, 32u> llvm_param_types;
        constexpr auto param_alignment = 16u;
        auto param_size_accum = static_cast<size_t>(0);
        for (auto i = 0u; i < arg_count; i++) {
            auto arg_type = f->arguments()[i]->type();
            LUISA_ASSERT(arg_type->alignment() < param_alignment, "Invalid argument alignment.");
            auto param_offset = luisa::align(param_size_accum, param_alignment);
            // pad if necessary
            if (param_offset > param_size_accum) {
                auto llvm_padding_type = llvm::ArrayType::get(llvm_i8_type, param_offset - param_size_accum);
                llvm_param_types.emplace_back(llvm_padding_type);
            }
            padded_param_indices.emplace_back(llvm_param_types.size());
            auto param_type = _translate_type(arg_type, false);
            llvm_param_types.emplace_back(param_type);
            param_size_accum = param_offset + arg_type->size();
        }
        // pad the last argument
        if (auto total_size = luisa::align(param_size_accum, param_alignment);
            total_size > param_size_accum) {
            auto llvm_padding_type = llvm::ArrayType::get(llvm_i8_type, total_size - param_size_accum);
            llvm_param_types.emplace_back(llvm_padding_type);
        }
        auto llvm_params_struct_type = llvm::StructType::create(_llvm_context, llvm_param_types, "LaunchParams");
        // load the params
        auto llvm_param_ptr = llvm_wrapper_function->getArg(0);
        llvm::SmallVector<llvm::Value *, 32u> llvm_args;
        for (auto i = 0u; i < arg_count; i++) {
            auto arg_type = f->arguments()[i]->type();
            auto padded_index = padded_param_indices[i];
            auto arg_name = luisa::format("arg{}", i);
            auto llvm_gep = b.CreateStructGEP(llvm_params_struct_type, llvm_param_ptr, padded_index,
                                              llvm::Twine{arg_name}.concat(".ptr"));
            auto llvm_arg_type = _translate_type(arg_type, true);
            auto llvm_arg = b.CreateAlignedLoad(llvm_arg_type, llvm_gep,
                                                llvm::MaybeAlign{arg_type->alignment()},
                                                llvm::Twine{arg_name});
            llvm_args.emplace_back(llvm_arg);
        }
        // load the launch config
        auto llvm_builtin_storage_type = _translate_type(Type::of<uint3>(), false);
        auto llvm_launch_config_struct_type = llvm::StructType::create(
            _llvm_context,
            {
                llvm_builtin_storage_type,// block_id
                llvm_builtin_storage_type,// dispatch_size
                llvm_builtin_storage_type,// block_size (optionally read)
            },
            "LaunchConfig");
        auto llvm_i32_type = llvm::Type::getInt32Ty(_llvm_context);
        auto llvm_builtin_type = llvm::VectorType::get(llvm_i32_type, 3, false);
        auto llvm_config_ptr = llvm_wrapper_function->getArg(1);
        auto load_builtin = [&](const char *name, size_t index) noexcept {
            auto llvm_gep = b.CreateStructGEP(llvm_launch_config_struct_type, llvm_config_ptr, index, llvm::Twine{name}.concat(".ptr"));
            return b.CreateAlignedLoad(llvm_builtin_type, llvm_gep, llvm::MaybeAlign{alignof(uint3)}, llvm::Twine{name});
        };
        auto llvm_block_id = load_builtin("block_id", 0);
        auto llvm_dispatch_size = load_builtin("dispatch_size", 1);
        auto static_block_size = f->block_size();
        auto llvm_block_size = !all(static_block_size == 0u) ?
                                   llvm::cast<llvm::Value>(_translate_literal(Type::of<uint3>(), &static_block_size, true)) :
                                   llvm::cast<llvm::Value>(load_builtin("block_size", 2));
        // compute number of threads per block
        auto llvm_block_size_x = b.CreateExtractElement(llvm_block_size, static_cast<uint64_t>(0));
        auto llvm_block_size_y = b.CreateExtractElement(llvm_block_size, static_cast<uint64_t>(1));
        auto llvm_block_size_z = b.CreateExtractElement(llvm_block_size, static_cast<uint64_t>(2));
        // hint that block size is power of two if we are using dynamic block size
        if (all(static_block_size == 0u)) {
            auto assume_power_of_two = [&](llvm::Value *v) noexcept {
                // power of two check: (v & (v - 1)) == 0
                auto v_minus_one = b.CreateNUWSub(v, b.getInt32(1));
                auto v_and_v_minus_one = b.CreateAnd(v, v_minus_one);
                auto is_zero = b.CreateICmpEQ(v_and_v_minus_one, b.getInt32(0));
                return b.CreateAssumption(is_zero);
            };
            assume_power_of_two(llvm_block_size_x);
            assume_power_of_two(llvm_block_size_y);
            assume_power_of_two(llvm_block_size_z);
        }
        auto llvm_thread_count = b.CreateNUWMul(llvm_block_size_x, llvm_block_size_y, "thread_count");
        llvm_thread_count = b.CreateNUWMul(llvm_thread_count, llvm_block_size_z);
        // thread-in-block loop
        auto llvm_ptr_i = b.CreateAlloca(llvm_i32_type, nullptr, "loop.i.ptr");
        b.CreateStore(b.getInt32(0), llvm_ptr_i);
        // loop head
        auto llvm_loop_block = llvm::BasicBlock::Create(_llvm_context, "loop.head", llvm_wrapper_function);
        b.CreateBr(llvm_loop_block);
        b.SetInsertPoint(llvm_loop_block);
        // compute thread id
        auto llvm_i = b.CreateLoad(llvm_i32_type, llvm_ptr_i, "loop.i");
        auto llvm_thread_id_x = b.CreateURem(llvm_i, llvm_block_size_x, "thread_id.x");
        auto llvm_thread_id_yz = b.CreateUDiv(llvm_i, llvm_block_size_x);
        auto llvm_thread_id_y = b.CreateURem(llvm_thread_id_yz, llvm_block_size_y, "thread_id.y");
        auto llvm_thread_id_z = b.CreateUDiv(llvm_thread_id_yz, llvm_block_size_y, "thread_id.z");
        auto llvm_thread_id = llvm::cast<llvm::Value>(llvm::PoisonValue::get(llvm_builtin_type));
        llvm_thread_id = b.CreateInsertElement(llvm_thread_id, llvm_thread_id_x, static_cast<uint64_t>(0));
        llvm_thread_id = b.CreateInsertElement(llvm_thread_id, llvm_thread_id_y, static_cast<uint64_t>(1));
        llvm_thread_id = b.CreateInsertElement(llvm_thread_id, llvm_thread_id_z, static_cast<uint64_t>(2));
        llvm_thread_id->setName("thread_id");
        // compute dispatch id
        auto llvm_dispatch_id = b.CreateNUWMul(llvm_block_id, llvm_block_size);
        llvm_dispatch_id = b.CreateNUWAdd(llvm_dispatch_id, llvm_thread_id, "dispatch_id");
        // check if in range
        auto llvm_in_range = b.CreateICmpULT(llvm_dispatch_id, llvm_dispatch_size);
        llvm_in_range = b.CreateAndReduce(llvm_in_range);
        llvm_in_range->setName("thread_id.in.range");
        // branch
        auto llvm_loop_body_block = llvm::BasicBlock::Create(_llvm_context, "loop.body", llvm_wrapper_function);
        auto llvm_loop_update_block = llvm::BasicBlock::Create(_llvm_context, "loop.update", llvm_wrapper_function);
        b.CreateCondBr(llvm_in_range, llvm_loop_body_block, llvm_loop_update_block);
        // loop body
        b.SetInsertPoint(llvm_loop_body_block);
        // call the kernel
        auto call_args = llvm_args;
        for (auto i = 0u; i < CurrentFunction::builtin_variable_count; i++) {
            switch (i) {
                case CurrentFunction::builtin_variable_index_thread_id: call_args.emplace_back(llvm_thread_id); break;
                case CurrentFunction::builtin_variable_index_block_id: call_args.emplace_back(llvm_block_id); break;
                case CurrentFunction::builtin_variable_index_dispatch_id: call_args.emplace_back(llvm_dispatch_id); break;
                case CurrentFunction::builtin_variable_index_block_size: call_args.emplace_back(llvm_block_size); break;
                case CurrentFunction::builtin_variable_index_dispatch_size: call_args.emplace_back(llvm_dispatch_size); break;
                default: LUISA_ERROR_WITH_LOCATION("Invalid builtin variable index.");
            }
        }
        b.CreateCall(llvm_kernel, call_args);
        b.CreateBr(llvm_loop_update_block);
        // loop update
        b.SetInsertPoint(llvm_loop_update_block);
        auto llvm_next_i = b.CreateNUWAdd(llvm_i, b.getInt32(1), "loop.i.next");
        b.CreateStore(llvm_next_i, llvm_ptr_i);
        auto llvm_loop_cond = b.CreateICmpULT(llvm_next_i, llvm_thread_count, "loop.cond");
        auto llvm_loop_merge_block = llvm::BasicBlock::Create(_llvm_context, "loop.merge", llvm_wrapper_function);
        b.CreateCondBr(llvm_loop_cond, llvm_loop_block, llvm_loop_merge_block);
        // loop merge
        b.SetInsertPoint(llvm_loop_merge_block);
        b.CreateRetVoid();

        return llvm_wrapper_function;
    }

    [[nodiscard]] llvm::Function *_translate_function_definition(const xir::FunctionDefinition *f,
                                                                 llvm::Function::LinkageTypes linkage,
                                                                 llvm::StringRef default_name) noexcept {
        auto llvm_ret_type = _translate_type(f->type(), true);
        llvm::SmallVector<llvm::Type *, 16u> llvm_arg_types;
        for (auto arg : f->arguments()) {
            if (arg->is_reference()) {
                // reference arguments are passed by pointer
                llvm_arg_types.emplace_back(llvm::PointerType::get(_llvm_context, 0));
            } else {
                // value and resource arguments are passed by value
                llvm_arg_types.emplace_back(_translate_type(arg->type(), true));
            }
        }
        // built-in variables
        auto llvm_i32_type = llvm::Type::getInt32Ty(_llvm_context);
        auto llvm_i32x3_type = llvm::VectorType::get(llvm_i32_type, 3, false);
        for (auto builtin = 0u; builtin < CurrentFunction::builtin_variable_count; builtin++) {
            llvm_arg_types.emplace_back(llvm_i32x3_type);
        }

        // create function
        auto llvm_func_type = llvm::FunctionType::get(llvm_ret_type, llvm_arg_types, false);
        auto func_name = _get_name_from_metadata(f, default_name);
        auto llvm_func = llvm::Function::Create(llvm_func_type, linkage, llvm::Twine{func_name}, _llvm_module);
        _llvm_functions.emplace(f, llvm_func);
        // create current translation context
        CurrentFunction current{.func = llvm_func};
        // map arguments
        auto arg_index = 0u;
        for (auto &llvm_arg : current.func->args()) {
            auto non_builtin_count = f->arguments().size();
            if (auto arg_i = arg_index++; arg_i < non_builtin_count) {
                auto arg = f->arguments()[arg_i];
                current.value_map.emplace(arg, &llvm_arg);
            } else {// built-in variable
                auto builtin = arg_i - non_builtin_count;
                switch (builtin) {
                    case CurrentFunction::builtin_variable_index_thread_id: llvm_arg.setName("thread_id"); break;
                    case CurrentFunction::builtin_variable_index_block_id: llvm_arg.setName("block_id"); break;
                    case CurrentFunction::builtin_variable_index_dispatch_id: llvm_arg.setName("dispatch_id"); break;
                    case CurrentFunction::builtin_variable_index_block_size: llvm_arg.setName("block_size"); break;
                    case CurrentFunction::builtin_variable_index_dispatch_size: llvm_arg.setName("dispatch_size"); break;
                    default: LUISA_ERROR_WITH_LOCATION("Invalid builtin variable index.");
                }
                current.builtin_variables[builtin] = &llvm_arg;
            }
        }
        // translate body
        static_cast<void>(_translate_basic_block(current, f->body_block()));
        // return
        return llvm_func;
    }

    [[nodiscard]] llvm::Function *_translate_callable_function(const xir::CallableFunction *f) noexcept {
        return _translate_function_definition(f, llvm::Function::PrivateLinkage, "callable");
    }

    [[nodiscard]] llvm::Function *_translate_function(const xir::Function *f) noexcept {
        if (auto iter = _llvm_functions.find(f); iter != _llvm_functions.end()) {
            return iter->second;
        }
        auto llvm_func = [&] {
            switch (f->derived_function_tag()) {
                case xir::DerivedFunctionTag::KERNEL:
                    return _translate_kernel_function(static_cast<const xir::KernelFunction *>(f));
                case xir::DerivedFunctionTag::CALLABLE:
                    return _translate_callable_function(static_cast<const xir::CallableFunction *>(f));
                case xir::DerivedFunctionTag::EXTERNAL: LUISA_NOT_IMPLEMENTED();
            }
            LUISA_ERROR_WITH_LOCATION("Invalid function type.");
        }();
        _llvm_functions.emplace(f, llvm_func);
        return llvm_func;
    }

public:
    explicit FallbackCodegen(llvm::LLVMContext &ctx) noexcept
        : _llvm_context{ctx} {}

    [[nodiscard]] auto emit(const xir::Module *module) noexcept {
        auto llvm_module = std::make_unique<llvm::Module>(llvm::StringRef{_get_name_from_metadata(module)}, _llvm_context);
        auto location_md = module->find_metadata<xir::LocationMD>();
        auto module_location = location_md ? location_md->file().string() : "unknown";
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
