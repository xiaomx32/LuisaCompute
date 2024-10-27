#pragma once

#include <luisa/core/stl/string.h>
#include <luisa/xir/metadata.h>

namespace luisa::compute::xir {

class LC_XIR_API CommentMD final : public Metadata {

private:
    luisa::string _comment;

public:
    explicit CommentMD(Pool *pool, luisa::string comment = {}) noexcept;
    [[nodiscard]] DerivedMetadataTag derived_metadata_tag() const noexcept override {
        return DerivedMetadataTag::COMMENT;
    }
    [[nodiscard]] auto &comment() noexcept { return _comment; }
    [[nodiscard]] auto &comment() const noexcept { return _comment; }
    void set_comment(luisa::string_view comment) noexcept;
};

}// namespace luisa::compute::xir
