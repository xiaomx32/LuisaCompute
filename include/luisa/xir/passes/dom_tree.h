#pragma once

#include <luisa/core/stl/unordered_map.h>
#include <luisa/xir/function.h>

namespace luisa::compute::xir {

class DomTree;

class LC_XIR_API DomTreeNode : public concepts::Noncopyable {

private:
    BasicBlock *_block;
    const DomTreeNode *_parent;
    luisa::vector<const DomTreeNode *> _children;
    luisa::vector<const DomTreeNode *> _frontiers;

public:
    explicit DomTreeNode(BasicBlock *block) noexcept;
    void add_child(DomTreeNode *child) noexcept;
    void add_frontier(DomTreeNode *frontier) noexcept;

public:
    [[nodiscard]] auto parent() const noexcept { return _parent; }
    [[nodiscard]] auto block() const noexcept { return _block; }
    [[nodiscard]] auto children() const noexcept { return luisa::span{_children}; }
    [[nodiscard]] auto frontiers() const noexcept { return luisa::span{_frontiers}; }
};

class LC_XIR_API DomTree {

private:
    luisa::unordered_map<BasicBlock *, luisa::unique_ptr<DomTreeNode>> _nodes;
    const DomTreeNode *_root;

public: /* for internal usage only */
    DomTree() noexcept;
    DomTreeNode *add_or_get_node(BasicBlock *block) noexcept;
    void set_root(DomTreeNode *root) noexcept;
    void compute_dominance_frontiers() noexcept;

public:
    [[nodiscard]] auto root() const noexcept { return _root; }
    [[nodiscard]] auto &nodes() const noexcept { return _nodes; }
    [[nodiscard]] auto node(BasicBlock *block) const noexcept -> const DomTreeNode *;
    [[nodiscard]] bool contains(BasicBlock *block) const noexcept;
    [[nodiscard]] bool dominates(BasicBlock *src, BasicBlock *dst) const noexcept;
    [[nodiscard]] bool strictly_dominates(BasicBlock *src, BasicBlock *dst) const noexcept;
    [[nodiscard]] auto immediate_dominator(BasicBlock *block) const noexcept -> BasicBlock *;
};

[[nodiscard]] LC_XIR_API DomTree compute_dom_tree(Function *function) noexcept;

}// namespace luisa::compute::xir
