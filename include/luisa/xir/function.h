#pragma once

#include <luisa/xir/argument.h>
#include <luisa/xir/basic_block.h>

namespace luisa::compute::xir {

enum struct FunctionTag {
    KERNEL,
    CALLABLE,
};

class LC_XIR_API Function : public IntrusiveForwardNode<Function, DerivedValue<DerivedValueTag::FUNCTION>> {

private:
    FunctionTag _function_tag;
    BasicBlock *_body;
    luisa::vector<Argument *> _arguments;

public:
    explicit Function(Pool *pool, FunctionTag tag,
                      const Type *type = nullptr) noexcept;

    [[nodiscard]] auto function_tag() const noexcept { return _function_tag; }

    void add_argument(Argument *argument) noexcept;
    void insert_argument(size_t index, Argument *argument) noexcept;
    void remove_argument(Argument *argument) noexcept;
    void remove_argument(size_t index) noexcept;
    void replace_argument(Argument *old_argument, Argument *new_argument) noexcept;
    void replace_argument(size_t index, Argument *argument) noexcept;

    Argument *create_argument(const Type *type, bool by_ref) noexcept;
    ValueArgument *create_value_argument(const Type *type) noexcept;
    ReferenceArgument *create_reference_argument(const Type *type) noexcept;

    [[nodiscard]] BasicBlock *body() noexcept { return _body; }
    [[nodiscard]] const BasicBlock *body() const noexcept { return _body; }

    [[nodiscard]] auto &arguments() noexcept { return _arguments; }
    [[nodiscard]] auto &arguments() const noexcept { return _arguments; }
};

class LC_XIR_API ExternalFunction : public IntrusiveForwardNode<ExternalFunction, Value> {

public:
    using Super::Super;
    [[nodiscard]] DerivedValueTag derived_value_tag() const noexcept override {
        return DerivedValueTag::EXTERNAL_FUNCTION;
    }
};

using FunctionList = IntrusiveForwardList<Function>;
using ExternalFunctionList = IntrusiveForwardList<ExternalFunction>;

}// namespace luisa::compute::xir
