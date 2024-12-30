#pragma once

#include <luisa/xir/instruction.h>

namespace luisa::compute::xir {

enum struct CastOp {
    STATIC_CAST,
    BITWISE_CAST,
};

[[nodiscard]] LC_XIR_API luisa::string_view to_string(CastOp op) noexcept;
[[nodiscard]] LC_XIR_API CastOp cast_op_from_string(luisa::string_view name) noexcept;

class LC_XIR_API CastInst final : public DerivedInstruction<DerivedInstructionTag::CAST> {

private:
    CastOp _op;

public:
    explicit CastInst(const Type *target_type = nullptr,
                      CastOp op = CastOp::STATIC_CAST,
                      Value *value = nullptr) noexcept;

    [[nodiscard]] auto op() const noexcept { return _op; }
    [[nodiscard]] Value *value() noexcept;
    [[nodiscard]] const Value *value() const noexcept;

    void set_op(CastOp op) noexcept;
    void set_value(Value *value) noexcept;
};

}// namespace luisa::compute::xir
