#include <luisa/xir/metadata/comment.h>

namespace luisa::compute::xir {

CommentMD::CommentMD(Pool *pool, luisa::string comment) noexcept
    : Metadata{pool}, _comment{std::move(comment)} {}

void CommentMD::set_comment(luisa::string_view comment) noexcept {
    _comment = comment;
}

}// namespace luisa::compute::xir