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
    explicit Argument(const Type *type = nullptr,
                      Function *parent_function = nullptr) noexcept;
    [[nodiscard]] Function *parent_function() noexcept { return _parent_function; }
    [[nodiscard]] const Function *parent_function() const noexcept { return _parent_function; }
};

using ValueArgument = DerivedValue<DerivedValueTag::VALUE_ARGUMENT, Argument>;
using ReferenceArgument = DerivedValue<DerivedValueTag::REFERENCE_ARGUMENT, Argument>;

using ArgumentList = InlineIntrusiveList<Argument>;

}// namespace luisa::compute::xir
