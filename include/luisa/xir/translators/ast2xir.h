#pragma once

#include <luisa/xir/module.h>

namespace luisa::compute {
class Function;
}// namespace luisa::compute

namespace luisa::compute::xir {

class AST2XIRContext;
using ASTFunction = compute::Function;

[[nodiscard]] LC_XIR_API AST2XIRContext *ast_to_xir_translate_begin() noexcept;
void LC_XIR_API ast_to_xir_translate_add(AST2XIRContext *ctx, const ASTFunction &f) noexcept;
[[nodiscard]] LC_XIR_API Module ast_to_xir_translate_finalize(AST2XIRContext *ctx) noexcept;

}// namespace luisa::compute::xir
