#pragma once

#include <luisa/core/stl/filesystem.h>
#include <luisa/xir/ilist.h>

namespace luisa::compute::xir {

enum struct DerivedMetadataTag {
    NAME,
    LOCATION,
    COMMENT,
};

class LC_XIR_API Metadata : public IntrusiveForwardNode<Metadata> {
public:
    using Super::Super;
    [[nodiscard]] virtual DerivedMetadataTag derived_metadata_tag() const noexcept = 0;
};

template<DerivedMetadataTag tag>
class LC_XIR_API DerivedMetadata : public Metadata {

public:
    using Metadata::Metadata;

    [[nodiscard]] static constexpr auto
    static_derived_metadata_tag() noexcept {
        return tag;
    }

    [[nodiscard]] DerivedMetadataTag
    derived_metadata_tag() const noexcept final {
        return static_derived_metadata_tag();
    }
};

class MetadataList : public IntrusiveForwardList<Metadata> {

private:
    Pool *_pool;

public:
    explicit MetadataList(Pool *pool) noexcept;
    [[nodiscard]] Metadata *find(DerivedMetadataTag tag) noexcept;
    [[nodiscard]] const Metadata *find(DerivedMetadataTag tag) const noexcept;
    [[nodiscard]] Metadata *find_or_create(DerivedMetadataTag tag) noexcept;
    void set_name(luisa::string_view name) noexcept;
    void set_location(const luisa::filesystem::path &file, int line, int column) noexcept;
    void add_comment(luisa::string_view comment) noexcept;
};

}// namespace luisa::compute::xir
