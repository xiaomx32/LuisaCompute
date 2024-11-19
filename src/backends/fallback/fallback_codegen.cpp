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
            case Type::Tag::BUFFER: LUISA_NOT_IMPLEMENTED();
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

    [[nodiscard]] llvm::Value *_translate_unary_logic_not(CurrentFunction &current, IRBuilder &b, const xir::Value *operand) noexcept {
        auto llvm_operand = _lookup_value(current, b, operand);
        auto operand_type = operand->type();
        LUISA_ASSERT(operand_type != nullptr, "Operand type is null.");
        LUISA_ASSERT(operand_type->is_scalar() || operand_type->is_vector(), "Invalid operand type.");
        auto llvm_cmp = _cmp_eq_zero(b, llvm_operand);
        auto llvm_i8_type = llvm::cast<llvm::Type>(llvm::Type::getInt8Ty(_llvm_context));
        if (operand_type->is_vector()) {
            auto dim = operand_type->dimension();
            llvm_i8_type = llvm::VectorType::get(llvm_i8_type, dim, false);
        }
        return b.CreateZExt(llvm_cmp, llvm_i8_type);
    }

    [[nodiscard]] llvm::Value *_translate_unary_bit_not(CurrentFunction &current, IRBuilder &b, const xir::Value *operand) noexcept {
        auto llvm_operand = _lookup_value(current, b, operand);
        LUISA_ASSERT(llvm_operand->getType()->isIntOrIntVectorTy() &&
                         !llvm_operand->getType()->isIntOrIntVectorTy(1),
                     "Invalid operand type.");
        return b.CreateNot(llvm_operand);
    }

    [[nodiscard]] llvm::Value *_translate_intrinsic_inst(CurrentFunction &current, IRBuilder &b, const xir::IntrinsicInst *inst) noexcept {
        switch (inst->op()) {
            case xir::IntrinsicOp::NOP: LUISA_ERROR_WITH_LOCATION("Unexpected NOP.");
            case xir::IntrinsicOp::UNARY_PLUS: return _translate_unary_plus(current, b, inst->operand(0u));
            case xir::IntrinsicOp::UNARY_MINUS: return _translate_unary_minus(current, b, inst->operand(0u));
            case xir::IntrinsicOp::UNARY_LOGIC_NOT: return _translate_unary_logic_not(current, b, inst->operand(0u));
            case xir::IntrinsicOp::UNARY_BIT_NOT: return _translate_unary_bit_not(current, b, inst->operand(0u));
            case xir::IntrinsicOp::BINARY_ADD: break;
            case xir::IntrinsicOp::BINARY_SUB: break;
            case xir::IntrinsicOp::BINARY_MUL: break;
            case xir::IntrinsicOp::BINARY_DIV: break;
            case xir::IntrinsicOp::BINARY_MOD: break;
            case xir::IntrinsicOp::BINARY_LOGIC_AND: break;
            case xir::IntrinsicOp::BINARY_LOGIC_OR: break;
            case xir::IntrinsicOp::BINARY_BIT_AND: break;
            case xir::IntrinsicOp::BINARY_BIT_OR: break;
            case xir::IntrinsicOp::BINARY_BIT_XOR: break;
            case xir::IntrinsicOp::BINARY_SHIFT_LEFT: break;
            case xir::IntrinsicOp::BINARY_SHIFT_RIGHT: break;
            case xir::IntrinsicOp::BINARY_ROTATE_LEFT: break;
            case xir::IntrinsicOp::BINARY_ROTATE_RIGHT: break;
            case xir::IntrinsicOp::BINARY_LESS: break;
            case xir::IntrinsicOp::BINARY_GREATER: break;
            case xir::IntrinsicOp::BINARY_LESS_EQUAL: break;
            case xir::IntrinsicOp::BINARY_GREATER_EQUAL: break;
            case xir::IntrinsicOp::BINARY_EQUAL: break;
            case xir::IntrinsicOp::BINARY_NOT_EQUAL: break;
            case xir::IntrinsicOp::ASSUME: break;
            case xir::IntrinsicOp::ASSERT: break;
            case xir::IntrinsicOp::THREAD_ID: break;
            case xir::IntrinsicOp::BLOCK_ID: break;
            case xir::IntrinsicOp::WARP_LANE_ID: break;
            case xir::IntrinsicOp::DISPATCH_ID: break;
            case xir::IntrinsicOp::KERNEL_ID: break;
            case xir::IntrinsicOp::OBJECT_ID: break;
            case xir::IntrinsicOp::BLOCK_SIZE: break;
            case xir::IntrinsicOp::WARP_SIZE: break;
            case xir::IntrinsicOp::DISPATCH_SIZE: break;
            case xir::IntrinsicOp::SYNCHRONIZE_BLOCK: break;
            case xir::IntrinsicOp::ALL: break;
            case xir::IntrinsicOp::ANY: break;
            case xir::IntrinsicOp::SELECT: break;
            case xir::IntrinsicOp::CLAMP: break;
            case xir::IntrinsicOp::SATURATE: break;
            case xir::IntrinsicOp::LERP: break;
            case xir::IntrinsicOp::SMOOTHSTEP: break;
            case xir::IntrinsicOp::STEP: break;
            case xir::IntrinsicOp::ABS: break;
            case xir::IntrinsicOp::MIN: break;
            case xir::IntrinsicOp::MAX: break;
            case xir::IntrinsicOp::CLZ: break;
            case xir::IntrinsicOp::CTZ: break;
            case xir::IntrinsicOp::POPCOUNT: break;
            case xir::IntrinsicOp::REVERSE: break;
            case xir::IntrinsicOp::ISINF: break;
            case xir::IntrinsicOp::ISNAN: break;
            case xir::IntrinsicOp::ACOS: break;
            case xir::IntrinsicOp::ACOSH: break;
            case xir::IntrinsicOp::ASIN: break;
            case xir::IntrinsicOp::ASINH: break;
            case xir::IntrinsicOp::ATAN: break;
            case xir::IntrinsicOp::ATAN2: break;
            case xir::IntrinsicOp::ATANH: break;
            case xir::IntrinsicOp::COS: break;
            case xir::IntrinsicOp::COSH: break;
            case xir::IntrinsicOp::SIN: break;
            case xir::IntrinsicOp::SINH: break;
            case xir::IntrinsicOp::TAN: break;
            case xir::IntrinsicOp::TANH: break;
            case xir::IntrinsicOp::EXP: break;
            case xir::IntrinsicOp::EXP2: break;
            case xir::IntrinsicOp::EXP10: break;
            case xir::IntrinsicOp::LOG: break;
            case xir::IntrinsicOp::LOG2: break;
            case xir::IntrinsicOp::LOG10: break;
            case xir::IntrinsicOp::POW: break;
            case xir::IntrinsicOp::POW_INT: break;
            case xir::IntrinsicOp::SQRT: break;
            case xir::IntrinsicOp::RSQRT: break;
            case xir::IntrinsicOp::CEIL: break;
            case xir::IntrinsicOp::FLOOR: break;
            case xir::IntrinsicOp::FRACT: break;
            case xir::IntrinsicOp::TRUNC: break;
            case xir::IntrinsicOp::ROUND: break;
            case xir::IntrinsicOp::FMA: break;
            case xir::IntrinsicOp::COPYSIGN: break;
            case xir::IntrinsicOp::CROSS: break;
            case xir::IntrinsicOp::DOT: break;
            case xir::IntrinsicOp::LENGTH: break;
            case xir::IntrinsicOp::LENGTH_SQUARED: break;
            case xir::IntrinsicOp::NORMALIZE: break;
            case xir::IntrinsicOp::FACEFORWARD: break;
            case xir::IntrinsicOp::REFLECT: break;
            case xir::IntrinsicOp::REDUCE_SUM: break;
            case xir::IntrinsicOp::REDUCE_PRODUCT: break;
            case xir::IntrinsicOp::REDUCE_MIN: break;
            case xir::IntrinsicOp::REDUCE_MAX: break;
            case xir::IntrinsicOp::OUTER_PRODUCT: break;
            case xir::IntrinsicOp::MATRIX_COMP_MUL: break;
            case xir::IntrinsicOp::DETERMINANT: break;
            case xir::IntrinsicOp::TRANSPOSE: break;
            case xir::IntrinsicOp::INVERSE: break;
            case xir::IntrinsicOp::ATOMIC_EXCHANGE: break;
            case xir::IntrinsicOp::ATOMIC_COMPARE_EXCHANGE: break;
            case xir::IntrinsicOp::ATOMIC_FETCH_ADD: break;
            case xir::IntrinsicOp::ATOMIC_FETCH_SUB: break;
            case xir::IntrinsicOp::ATOMIC_FETCH_AND: break;
            case xir::IntrinsicOp::ATOMIC_FETCH_OR: break;
            case xir::IntrinsicOp::ATOMIC_FETCH_XOR: break;
            case xir::IntrinsicOp::ATOMIC_FETCH_MIN: break;
            case xir::IntrinsicOp::ATOMIC_FETCH_MAX: break;
            case xir::IntrinsicOp::BUFFER_READ: break;
            case xir::IntrinsicOp::BUFFER_WRITE: break;
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
            case xir::IntrinsicOp::RASTER_DISCARD: break;
            case xir::IntrinsicOp::RASTER_DDX: break;
            case xir::IntrinsicOp::RASTER_DDY: break;
            case xir::IntrinsicOp::WARP_IS_FIRST_ACTIVE_LANE: break;
            case xir::IntrinsicOp::WARP_FIRST_ACTIVE_LANE: break;
            case xir::IntrinsicOp::WARP_ACTIVE_ALL_EQUAL: break;
            case xir::IntrinsicOp::WARP_ACTIVE_BIT_AND: break;
            case xir::IntrinsicOp::WARP_ACTIVE_BIT_OR: break;
            case xir::IntrinsicOp::WARP_ACTIVE_BIT_XOR: break;
            case xir::IntrinsicOp::WARP_ACTIVE_COUNT_BITS: break;
            case xir::IntrinsicOp::WARP_ACTIVE_MAX: break;
            case xir::IntrinsicOp::WARP_ACTIVE_MIN: break;
            case xir::IntrinsicOp::WARP_ACTIVE_PRODUCT: break;
            case xir::IntrinsicOp::WARP_ACTIVE_SUM: break;
            case xir::IntrinsicOp::WARP_ACTIVE_ALL: break;
            case xir::IntrinsicOp::WARP_ACTIVE_ANY: break;
            case xir::IntrinsicOp::WARP_ACTIVE_BIT_MASK: break;
            case xir::IntrinsicOp::WARP_PREFIX_COUNT_BITS: break;
            case xir::IntrinsicOp::WARP_PREFIX_SUM: break;
            case xir::IntrinsicOp::WARP_PREFIX_PRODUCT: break;
            case xir::IntrinsicOp::WARP_READ_LANE: break;
            case xir::IntrinsicOp::WARP_READ_FIRST_ACTIVE_LANE: break;
            case xir::IntrinsicOp::INDIRECT_DISPATCH_SET_KERNEL: break;
            case xir::IntrinsicOp::INDIRECT_DISPATCH_SET_COUNT: break;
            case xir::IntrinsicOp::SHADER_EXECUTION_REORDER: break;
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
                auto llvm_src_type = _translate_type(src_type, true);
                auto llvm_dst_type = _translate_type(dst_type, true);
                if (dst_elem_type->is_bool()) {
                    auto cmp = _cmp_ne_zero(b, llvm_value);
                    return b.CreateZExt(cmp, llvm_dst_type);
                }
                // general case
                auto classify = [](const Type *t) noexcept {
                    return std::make_tuple(
                        t->is_float16() || t->is_float32() || t->is_float64(),
                        t->is_int8() || t->is_int16() || t->is_int32() || t->is_int64(),
                        t->is_uint8() || t->is_uint16() || t->is_uint32() || t->is_uint64() || t->is_bool());
                };
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
                return b.CreateCall(external_assert, {llvm_condition, llvm_message});
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
        return nullptr;
    }

    [[nodiscard]] llvm::Function *_translate_callable_function(const xir::CallableFunction *f) noexcept {
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
        // create function
        auto llvm_func_type = llvm::FunctionType::get(llvm_ret_type, llvm_arg_types, false);
        auto name_md = f->find_metadata<xir::NameMD>();
        auto func_name = name_md ? name_md->name() : "callable";
        auto llvm_func = llvm::Function::Create(llvm_func_type, llvm::Function::PrivateLinkage, llvm::Twine{func_name}, _llvm_module);
        _llvm_functions.emplace(f, llvm_func);
        // create current translation context
        CurrentFunction current{.func = llvm_func};
        // map arguments
        auto arg_index = 0u;
        for (auto &llvm_arg : current.func->args()) {
            auto arg = f->arguments()[arg_index++];
            current.value_map.emplace(arg, &llvm_arg);
        }
        // translate body
        static_cast<void>(_translate_basic_block(current, f->body_block()));
        // return
        return llvm_func;
    }

    [[nodiscard]] llvm::Function *_translate_function(const xir::Function *f) noexcept {
        if (auto iter = _llvm_functions.find(f); iter != _llvm_functions.end()) {
            return iter->second;
        }
        auto llvm_func = [&] {
            switch (f->derived_function_tag()) {
                case xir::DerivedFunctionTag::KERNEL:
                    return _translate_kernel_function(
                        static_cast<const xir::KernelFunction *>(f));
                case xir::DerivedFunctionTag::CALLABLE:
                    return _translate_callable_function(
                        static_cast<const xir::CallableFunction *>(f));
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
