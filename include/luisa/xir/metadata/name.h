#pragma once

#include <luisa/core/stl/string.h>
#include <luisa/xir/metadata.h>

namespace luisa::compute::xir {

class LC_XIR_API NameMD final : public Metadata {

private:
    luisa::string _name;

public:
    explicit NameMD(Pool *pool, luisa::string name = {}) noexcept;
    [[nodiscard]] DerivedMetadataTag derived_metadata_tag() const noexcept override {
        return DerivedMetadataTag::NAME;
    }
    void set_name(luisa::string_view name) noexcept;
    [[nodiscard]] auto &name() noexcept { return _name; }
    [[nodiscard]] auto &name() const noexcept { return _name; }
};

}// namespace luisa::compute::xir
