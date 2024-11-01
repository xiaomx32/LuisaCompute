#include <luisa/xir/translators/ast2xir.h>

namespace luisa::compute::xir {

AST2XIRContext *ast_to_xir_translate_begin() noexcept {
    return nullptr;
}

void ast_to_xir_translate_add(AST2XIRContext *ctx, const ASTFunction &f) noexcept {
}

Module ast_to_xir_translate_finalize(AST2XIRContext *ctx) noexcept {
    return {};
}

}// namespace luisa::compute::xir
