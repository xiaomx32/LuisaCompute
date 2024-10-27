#include <luisa/core/logging.h>
#include <luisa/xir/metadata/name.h>
#include <luisa/xir/metadata/location.h>
#include <luisa/xir/metadata/comment.h>
#include <luisa/xir/metadata.h>

namespace luisa::compute::xir {

MetadataList::MetadataList(Pool *pool) noexcept
    : _pool{pool} {}

Metadata *MetadataList::find(DerivedMetadataTag tag) noexcept {
    for (auto &m : *this) {
        if (m.derived_metadata_tag() == tag) {
            return &m;
        }
    }
    return nullptr;
}

const Metadata *MetadataList::find(DerivedMetadataTag tag) const noexcept {
    return const_cast<MetadataList *>(this)->find(tag);
}

Metadata *MetadataList::find_or_create(DerivedMetadataTag tag) noexcept {
    for (auto &m : *this) {
        if (m.derived_metadata_tag() == tag) {
            return &m;
        }
    }
    switch (tag) {
        case DerivedMetadataTag::NAME: return _pool->create<NameMD>();
        case DerivedMetadataTag::LOCATION: return _pool->create<LocationMD>();
        case DerivedMetadataTag::COMMENT: return _pool->create<CommentMD>();
    }
    LUISA_ERROR_WITH_LOCATION(
        "Unknown derived metadata tag 0x{:x}.",
        static_cast<uint32_t>(tag));
}

void MetadataList::set_name(luisa::string_view name) noexcept {
    auto m = static_cast<NameMD *>(find_or_create(DerivedMetadataTag::NAME));
    m->set_name(name);
}

void MetadataList::set_location(const luisa::filesystem::path &file, int line, int column) noexcept {
    auto m = static_cast<LocationMD *>(find_or_create(DerivedMetadataTag::LOCATION));
    m->set_file(file);
    m->set_line(line);
    m->set_column(column);
}

void MetadataList::add_comment(luisa::string_view comment) noexcept {
    auto m = _pool->create<CommentMD>(luisa::string{comment});
    m->add_to_list(*this);
}

}// namespace luisa::compute::xir
