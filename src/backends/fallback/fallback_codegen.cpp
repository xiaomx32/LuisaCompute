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
    luisa::unordered_map<const Type *, llvm::Constant *> _llvm_zeros;

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
        struct_type->type = ::llvm::StructType::create(_llvm_context, field_types);
        struct_type->padded_field_indices = std::move(field_indices);
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
                if (dim > 2u && (elem_type->is_int64() || elem_type->is_uint64())) {
                    LUISA_ERROR_WITH_LOCATION("64-bit integer vector with dimension > 2 is not implemented.");
                }
                llvm::SmallVector<llvm::Constant *, 4u> elements;
                for (auto i = 0u; i < dim; i++) {
                    auto elem_data = static_cast<const std::byte *>(data) + i * stride;
                    elements.emplace_back(_translate_literal(elem_type, elem_data));
                }
                return llvm::ConstantVector::get(elements);
            }
            case Type::Tag::MATRIX: {
                LUISA_ASSERT(llvm_type->isArrayTy(), "Matrix type should be an array type.");
                auto dim = t->dimension();
                auto col_type = Type::vector(t->element(), dim);
                auto col_stride = col_type->size();
                llvm::SmallVector<llvm::Constant *, 4u> elements;
                for (auto i = 0u; i < dim; i++) {
                    auto col_data = static_cast<const std::byte *>(data) + i * col_stride;
                    elements.emplace_back(_translate_literal(col_type, col_data));
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
                    elements.emplace_back(_translate_literal(elem_type, elem_data));
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
                    fields[padded_index] = _translate_literal(field_type, field_data);
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
        auto llvm_value = _translate_literal(type, c->data());
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
                    auto llvm_type = _translate_type(v->type());
                    return b.CreateLoad(llvm_type, c);
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

    [[nodiscard]] llvm::Value *_translate_intrinsic_inst(CurrentFunction &current, IRBuilder &b, const xir::IntrinsicInst *inst) noexcept {

        // extract needs special handling
        if (inst->op() == xir::IntrinsicOp::EXTRACT) {
            auto base = inst->operand(0u);
            auto indices = inst->operand_uses().subspan(1u);
            auto llvm_base = _lookup_value(current, b, base, false);
            auto llvm_base_type = _translate_type(base->type());
            llvm::SmallVector<llvm::Value *> llvm_indices;
            llvm_indices.reserve(1 + indices.size());
            llvm_indices.emplace_back(b.getInt64(0));
            _collect_aggregate_mapped_indices(current, b, base->type(), indices, llvm_indices);
            // we have to create a temporary alloca for non-constant indices
            // TODO: optimize this
            if (!llvm_base->getType()->isPointerTy()) {
                auto temp = b.CreateAlloca(llvm_base_type);
                b.CreateStore(llvm_base, temp);
                llvm_base = temp;
            }
            auto llvm_gep = b.CreateInBoundsGEP(llvm_base_type, llvm_base, llvm_indices);
            auto llvm_type = _translate_type(inst->type());
            return b.CreateLoad(llvm_type, llvm_gep);
        }
        auto llvm_type = _translate_type(inst->type());
        return llvm::UndefValue::get(llvm_type);
        LUISA_NOT_IMPLEMENTED();
    }

    void _collect_aggregate_mapped_indices(CurrentFunction &current, IRBuilder &b,
                                           const Type *t, luisa::span<const xir::Use *const> indices,
                                           llvm::SmallVector<llvm::Value *> &collected) noexcept {
        // degenerate cases
        if (t->is_scalar()) {
            LUISA_ASSERT(indices.empty(), "Scalar type should have no indices.");
            return;
        }
        if (indices.empty()) { return; }

        // index into current aggregate and update indices
        auto index = indices.front()->value();
        indices = indices.subspan(1);
        LUISA_ASSERT(index != nullptr && index->type() != nullptr, "Index is null.");

        // structures only allow static indices and need special handling for padding
        if (t->is_structure()) {
            LUISA_ASSERT(index->derived_value_tag() == xir::DerivedValueTag::CONSTANT,
                         "Structure index should be constant.");
            auto i = [index = static_cast<const xir::Constant *>(index)]() noexcept -> uint64_t {
                auto index_t = index->type();
                LUISA_ASSERT(index_t != nullptr, "Index type is null.");
                switch (index_t->tag()) {
                    case Type::Tag::INT8: return index->as<int8_t>();
                    case Type::Tag::UINT8: return index->as<uint8_t>();
                    case Type::Tag::INT16: return index->as<int16_t>();
                    case Type::Tag::UINT16: return index->as<uint16_t>();
                    case Type::Tag::INT32: return index->as<int32_t>();
                    case Type::Tag::UINT32: return index->as<uint32_t>();
                    case Type::Tag::INT64: return index->as<int64_t>();
                    case Type::Tag::UINT64: return index->as<uint64_t>();
                    default: break;
                }
                LUISA_ERROR_WITH_LOCATION("Invalid index type: {}.", index_t->description());
            }();
            LUISA_ASSERT(i < t->members().size(), "Index out of range.");
            // remap index to padded index
            auto llvm_struct = _translate_struct_type(t);
            auto padded_index = llvm_struct->padded_field_indices[i];
            collected.emplace_back(b.getInt64(padded_index));
            // update type
            t = t->members()[i];
        } else {
            collected.emplace_back(_lookup_value(current, b, index));
            if (t->is_matrix()) {
                t = Type::vector(t->element(), t->dimension());
            } else {
                LUISA_ASSERT(t->is_array() || t->is_vector(), "Invalid aggregate type.");
                t = t->element();
            }
        }
        // recursively collect indices
        _collect_aggregate_mapped_indices(current, b, t, indices, collected);
    }

    [[nodiscard]] llvm::Value *_translate_gep_inst(CurrentFunction &current, IRBuilder &b, const xir::GEPInst *inst) noexcept {
        auto base = _lookup_value(current, b, inst->base());
        LUISA_ASSERT(base->getType()->isPointerTy(), "Base should be a pointer.");
        auto llvm_type = _translate_type(inst->type());
        llvm::SmallVector<llvm::Value *> indices;
        indices.reserve(1 /* 0 that dereference the ptr */ + inst->index_count());
        indices.emplace_back(b.getInt64(0));
        _collect_aggregate_mapped_indices(current, b, inst->base()->type(), inst->index_uses(), indices);
        return b.CreateInBoundsGEP(llvm_type, base, indices);
    }

    [[nodiscard]] llvm::Constant *_get_constant_zero(const Type *t) noexcept {
        auto iter = _llvm_zeros.try_emplace(t, nullptr).first;
        if (iter->second == nullptr) {
            if (t->is_scalar()) {
                auto zero_data = 0ull;
                iter->second = _translate_literal(t, &zero_data);
            } else {
                auto llvm_type = _translate_type(t);
                iter->second = llvm::ConstantAggregateZero::get(llvm_type);
            }
        }
        return iter->second;
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
                auto llvm_type = _translate_type(inst->type());
                return b.CreateAlloca(llvm_type);
            }
            case xir::DerivedInstructionTag::LOAD: {
                auto load_inst = static_cast<const xir::LoadInst *>(inst);
                auto llvm_type = _translate_type(load_inst->type());
                auto llvm_ptr = _lookup_value(current, b, load_inst->variable());
                return b.CreateLoad(llvm_type, llvm_ptr);
            }
            case xir::DerivedInstructionTag::STORE: {
                auto store_inst = static_cast<const xir::StoreInst *>(inst);
                auto llvm_ptr = _lookup_value(current, b, store_inst->variable());
                auto llvm_value = _lookup_value(current, b, store_inst->value());
                return b.CreateStore(llvm_value, llvm_ptr);
            }
            case xir::DerivedInstructionTag::GEP: {
                auto gep_inst = static_cast<const xir::GEPInst *>(inst);
                return _translate_gep_inst(current, b, gep_inst);
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
                auto llvm_value = _lookup_value(current, b, cast_inst->value());
                switch (cast_inst->op()) {
                    case xir::CastOp::STATIC_CAST: {
                        auto dst_type = cast_inst->type();
                        auto src_type = cast_inst->value()->type();
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
                        auto llvm_dst_type = _translate_type(dst_type);
                        if (dst_elem_type->is_bool()) {
                            auto zero = _get_constant_zero(src_elem_type);
                            auto cmp = src_elem_type->is_float16() || src_elem_type->is_float32() || src_elem_type->is_float64() ?
                                           b.CreateFCmpONE(llvm_value, zero) :
                                           b.CreateICmpNE(llvm_value, zero);
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
                        auto llvm_type = _translate_type(cast_inst->type());
                        return b.CreateBitCast(llvm_value, llvm_type);
                    }
                }
                LUISA_ERROR_WITH_LOCATION("Invalid cast operation.");
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
        auto llvm_ret_type = _translate_type(f->type());
        llvm::SmallVector<llvm::Type *, 16u> llvm_arg_types;
        for (auto arg : f->arguments()) {
            if (arg->is_reference()) {
                // reference arguments are passed by pointer
                llvm_arg_types.emplace_back(llvm::PointerType::get(_llvm_context, 0));
            } else {
                // value and resource arguments are passed by value
                llvm_arg_types.emplace_back(_translate_type(arg->type()));
            }
        }
        // create function
        auto llvm_func_type = llvm::FunctionType::get(llvm_ret_type, llvm_arg_types, false);
        auto name_md = f->find_metadata<xir::NameMD>();
        auto func_name = name_md ? name_md->name() : "callable";
        auto llvm_func = llvm::Function::Create(llvm_func_type, llvm::Function::InternalLinkage, llvm::Twine{func_name}, _llvm_module);
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
