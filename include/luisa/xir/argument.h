#pragma once

#include <luisa/xir/value.h>

namespace luisa::compute::xir {

class Function;

class LC_XIR_API Argument : public Value {

private:
    Function *_parent_function = nullptr;

private:
    friend class Function;
    void _set_parent_function(Function *func) noexcept;

public:
    explicit Argument(Pool *pool, const Type *type = nullptr,
                      Function *parent_function = nullptr) noexcept;
    [[nodiscard]] Function *parent_function() noexcept { return _parent_function; }
    [[nodiscard]] const Function *parent_function() const noexcept { return _parent_function; }
};

class LC_XIR_API ValueArgument final : public Argument {
public:
    using Argument::Argument;
    [[nodiscard]] DerivedValueTag derived_value_tag() const noexcept override {
        return DerivedValueTag::VALUE_ARGUMENT;
    }
};

class LC_XIR_API ReferenceArgument final : public Argument {
public:
    using Argument::Argument;
    [[nodiscard]] DerivedValueTag derived_value_tag() const noexcept override {
        return DerivedValueTag::REFERENCE_ARGUMENT;
    }
};

using ArgumentList = InlineIntrusiveList<Argument>;

}// namespace luisa::compute::xir
