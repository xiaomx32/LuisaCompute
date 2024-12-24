#pragma once

#include <luisa/ast/type_registry.h>
#include <luisa/xir/constant.h>
#include <luisa/xir/function.h>

namespace luisa::compute::xir {

class Constant;

class LC_XIR_API Module : public PooledObject,
                          public MetadataMixin {

private:
    FunctionList _functions;
    ConstantList _constants;

public:
    Module() noexcept;

    void add_function(Function *function) noexcept;
    [[nodiscard]] KernelFunction *create_kernel() noexcept;
    [[nodiscard]] CallableFunction *create_callable(const Type *ret_type) noexcept;
    [[nodiscard]] ExternalFunction *create_external_function(const Type *ret_type) noexcept;
    [[nodiscard]] auto &functions() noexcept { return _functions; }
    [[nodiscard]] auto &functions() const noexcept { return _functions; }

    void add_constant(Constant *constant) noexcept;
    [[nodiscard]] Constant *create_constant(const Type *type, const void *data = nullptr) noexcept;
    [[nodiscard]] Constant *create_constant_zero(const Type *type) noexcept;
    [[nodiscard]] Constant *create_constant_one(const Type *type) noexcept;
    [[nodiscard]] auto &constants() noexcept { return _constants; }
    [[nodiscard]] auto &constants() const noexcept { return _constants; }
};

}// namespace luisa::compute::xir
