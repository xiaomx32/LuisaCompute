#pragma once

#include <luisa/ast/type_registry.h>
#include <luisa/xir/function.h>

namespace luisa::compute::xir {

class Constant;

class LC_XIR_API Module : public MetadataMixin<Module> {

private:
    Pool _pool;
    FunctionList _functions;

public:
    Module() noexcept;

    [[nodiscard]] Function *create_kernel() noexcept;
    [[nodiscard]] Function *create_callable(const Type *ret_type) noexcept;
    [[nodiscard]] Constant *create_constant(const Type *type, const void *data) noexcept;

    template<typename T>
    [[nodiscard]] Constant *create_constant(const T &value) noexcept {
        return create_constant(Type::of<T>(), &value);
    }

    void add_function(Function *function) noexcept;

    [[nodiscard]] auto pool() noexcept { return &_pool; }
    [[nodiscard]] auto pool() const noexcept { return &_pool; }
    [[nodiscard]] auto &functions() noexcept { return _functions; }
    [[nodiscard]] auto &functions() const noexcept { return _functions; }
};

}// namespace luisa::compute::xir
