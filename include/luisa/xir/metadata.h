#pragma once

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

using MetadataList = IntrusiveForwardList<Metadata>;

}// namespace luisa::compute::xir
