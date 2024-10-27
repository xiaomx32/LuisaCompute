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

template<DerivedMetadataTag tag, typename Base = Metadata>
class LC_XIR_API DerivedMetadata : public Base {

public:
    using Base::Base;

    [[nodiscard]] static constexpr auto
    static_derived_metadata_tag() noexcept {
        return tag;
    }

    [[nodiscard]] DerivedMetadataTag
    derived_metadata_tag() const noexcept final {
        return static_derived_metadata_tag();
    }
};

using MetadataList = IntrusiveForwardList<Metadata>;

namespace detail {
[[nodiscard]] LC_XIR_API Metadata *metadata_find(DerivedMetadataTag tag, MetadataList &list) noexcept;
[[nodiscard]] LC_XIR_API Metadata *metadata_create(Pool *pool, DerivedMetadataTag tag, MetadataList &list) noexcept;
[[nodiscard]] LC_XIR_API Metadata *metadata_find_or_create(Pool *pool, DerivedMetadataTag tag, MetadataList &list) noexcept;
LC_XIR_API void metadata_set_or_create_name(Pool *pool, MetadataList &list, luisa::string_view name) noexcept;
LC_XIR_API void metadata_set_or_create_location(Pool *pool, MetadataList &list, const luisa::filesystem::path &file, int line) noexcept;
LC_XIR_API void metadata_add_comment(Pool *pool, MetadataList &list, luisa::string_view comment) noexcept;
}// namespace detail

template<typename Parent>
class MetadataMixin {

private:
    MetadataList _metadata_list;

private:
    [[nodiscard]] Pool *_get_pool_from_parent() noexcept {
        return static_cast<Parent *>(this)->pool();
    }

protected:
    MetadataMixin() noexcept = default;
    ~MetadataMixin() noexcept = default;

public:
    [[nodiscard]] auto &metadata_list() noexcept { return _metadata_list; }
    [[nodiscard]] auto &metadata_list() const noexcept { return _metadata_list; }
    [[nodiscard]] auto find_metadata(DerivedMetadataTag tag) noexcept -> Metadata * {
        return detail::metadata_find(tag, _metadata_list);
    }
    [[nodiscard]] auto find_metadata(DerivedMetadataTag tag) const noexcept -> const Metadata * {
        return detail::metadata_find(tag, const_cast<MetadataList &>(_metadata_list));
    }
    [[nodiscard]] auto create_metadata(DerivedMetadataTag tag) noexcept {
        return detail::metadata_create(_get_pool_from_parent(), tag, _metadata_list);
    }
    [[nodiscard]] auto find_or_create_metadata(DerivedMetadataTag tag) noexcept {
        return detail::metadata_find_or_create(_get_pool_from_parent(), tag, _metadata_list);
    }
    template<typename T>
    [[nodiscard]] auto find_metadata() noexcept {
        return static_cast<T *>(find_metadata(T::static_derived_metadata_tag()));
    }
    template<typename T>
    [[nodiscard]] auto find_metadata() const noexcept {
        return static_cast<const T *>(find_metadata(T::static_derived_metadata_tag()));
    }
    template<typename T>
    [[nodiscard]] auto create_metadata() noexcept {
        return static_cast<T *>(create_metadata(T::static_derived_metadata_tag()));
    }
    template<typename T>
    [[nodiscard]] auto find_or_create_metadata() noexcept {
        return static_cast<T *>(find_or_create_metadata(T::static_derived_metadata_tag()));
    }
    void set_name(std::string_view name) noexcept {
        detail::metadata_set_or_create_name(_get_pool_from_parent(), _metadata_list, name);
    }
    void set_location(const std::filesystem::path &file, int line = -1) noexcept {
        detail::metadata_set_or_create_location(_get_pool_from_parent(), _metadata_list, file, line);
    }
    void add_comment(std::string_view comment) noexcept {
        detail::metadata_add_comment(_get_pool_from_parent(), _metadata_list, comment);
    }
};

}// namespace luisa::compute::xir
