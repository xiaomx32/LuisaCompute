#pragma once

#include <luisa/core/stl/memory.h>
#include <luisa/core/stl/vector.h>
#include <luisa/xir/module.h>

namespace luisa::compute::xir {

class IfInst;
class SwitchInst;
class LoopInst;
class SimpleLoopInst;
class RayQueryInst;
class BreakInst;
class ContinueInst;
class ReturnInst;
class UnreachableInst;
class BasicBlock;

// This pass extracts the control-flow graph (CFG) from the XIR module.
// The CFG is represented as an immutable tree of control-flow instructions and their nested blocks.

// CFGNodeTag is a tag for marking the subclass of CFGNode.
// We include the `CFG_` prefix to avoid potential macro conflicts.
enum class CFGNodeTag {
    CFG_TRIVIAL,
    CFG_SCOPE,
    CFG_IF,
    CFG_SWITCH,
    CFG_LOOP,
    CFG_SIMPLE_LOOP,
    CFG_RAY_QUERY,
    CFG_BREAK,
    CFG_CONTINUE,
    CFG_RETURN,
    CFG_UNREACHABLE,
};

class CFGNode {
public:
    virtual ~CFGNode() noexcept = default;
    [[nodiscard]] virtual CFGNodeTag tag() const noexcept = 0;
};

template<CFGNodeTag STATIC_TAG, typename Underlying = void>
class CFGDerivedNode : public CFGDerivedNode<STATIC_TAG> {

private:
    Underlying *_underlying;

public:
    explicit CFGDerivedNode(Underlying *underlying) noexcept : _underlying{underlying} {}
    [[nodiscard]] auto underlying() noexcept { return _underlying; }
    [[nodiscard]] auto underlying() const noexcept { return _underlying; }
};

template<CFGNodeTag STATIC_TAG>
class CFGDerivedNode<STATIC_TAG, void> : public CFGNode {
public:
    [[nodiscard]] static constexpr auto static_tag() noexcept { return STATIC_TAG; }
    [[nodiscard]] CFGNodeTag tag() const noexcept override { return static_tag(); }
};

class CFGTrivialNode : public CFGDerivedNode<CFGNodeTag::CFG_TRIVIAL, Instruction> {
public:
    using CFGDerivedNode::CFGDerivedNode;
};

// A scope is a list of CFG nodes that are executed in order.
class CFGScope : public CFGDerivedNode<CFGNodeTag::CFG_SCOPE> {

private:
    luisa::vector<luisa::unique_ptr<CFGNode>> _nodes;

public:
    [[nodiscard]] auto nodes() const noexcept {
        return luisa::span{_nodes};
    }

    void add(luisa::unique_ptr<CFGNode> node) noexcept {
        _nodes.emplace_back(std::move(node));
    }

    template<typename T, typename... Args>
        requires std::derived_from<T, CFGNode>
    void add(Args &&...args) noexcept {
        add(luisa::make_unique<T>(std::forward<Args>(args)...));
    }
};

class CFGIfNode : public CFGDerivedNode<CFGNodeTag::CFG_IF, IfInst> {
private:
    CFGScope _true;
    CFGScope _false;

public:
    using CFGDerivedNode::CFGDerivedNode;
    [[nodiscard]] auto true_scope() noexcept { return &_true; }
    [[nodiscard]] auto true_scope() const noexcept { return &_true; }
    [[nodiscard]] auto false_scope() noexcept { return &_false; }
    [[nodiscard]] auto false_scope() const noexcept { return &_false; }
};

class CFGSwitchNode : public CFGDerivedNode<CFGNodeTag::CFG_SWITCH, SwitchInst> {

public:
    struct Case {
        int64_t value;
        CFGScope scope;
    };

private:
    luisa::vector<luisa::unique_ptr<Case>> _cases;
    CFGScope _default;

public:
    using CFGDerivedNode::CFGDerivedNode;
    [[nodiscard]] auto cases() const noexcept { return luisa::span{_cases}; }
    [[nodiscard]] auto add_case(int64_t value) noexcept {
        auto result = luisa::make_unique<Case>(Case{.value = value});
        return &_cases.emplace_back(std::move(result))->scope;
    }
    [[nodiscard]] auto case_value(size_t i) const noexcept { return _cases[i]->value; }
    [[nodiscard]] auto case_scope(size_t i) noexcept { return &_cases[i]->scope; }
    [[nodiscard]] auto case_scope(size_t i) const noexcept { return &_cases[i]->scope; }
    [[nodiscard]] auto default_scope() noexcept { return &_default; }
    [[nodiscard]] auto default_scope() const noexcept { return &_default; }
};

class CFGLoopNode : public CFGDerivedNode<CFGNodeTag::CFG_LOOP, LoopInst> {

private:
    CFGScope _prepare;
    CFGScope _body;
    CFGScope _update;

public:
    using CFGDerivedNode::CFGDerivedNode;
    [[nodiscard]] auto prepare_scope() noexcept { return &_prepare; }
    [[nodiscard]] auto prepare_scope() const noexcept { return &_prepare; }
    [[nodiscard]] auto body_scope() noexcept { return &_body; }
    [[nodiscard]] auto body_scope() const noexcept { return &_body; }
    [[nodiscard]] auto update_scope() noexcept { return &_update; }
    [[nodiscard]] auto update_scope() const noexcept { return &_update; }
};

class CFGSimpleLoopNode : public CFGDerivedNode<CFGNodeTag::CFG_SIMPLE_LOOP, SimpleLoopInst> {

private:
    CFGScope _body;

public:
    using CFGDerivedNode::CFGDerivedNode;
    [[nodiscard]] auto body_scope() noexcept { return &_body; }
    [[nodiscard]] auto body_scope() const noexcept { return &_body; }
};

class CFGRayQueryNode : public CFGDerivedNode<CFGNodeTag::CFG_RAY_QUERY, RayQueryInst> {

private:
    CFGScope _surface;
    CFGScope _procedural;

public:
    using CFGDerivedNode::CFGDerivedNode;
    [[nodiscard]] auto surface_scope() noexcept { return &_surface; }
    [[nodiscard]] auto surface_scope() const noexcept { return &_surface; }
    [[nodiscard]] auto procedural_scope() noexcept { return &_procedural; }
    [[nodiscard]] auto procedural_scope() const noexcept { return &_procedural; }
};

using CFGBreakNode = CFGDerivedNode<CFGNodeTag::CFG_BREAK, BreakInst>;
using CFGContinueNode = CFGDerivedNode<CFGNodeTag::CFG_CONTINUE, ContinueInst>;
using CFGReturnNode = CFGDerivedNode<CFGNodeTag::CFG_RETURN, ReturnInst>;
using CFGUnreachableNode = CFGDerivedNode<CFGNodeTag::CFG_UNREACHABLE, UnreachableInst>;

class CFG {
private:
    CFGScope _entry;

public:
    [[nodiscard]] auto entry_scope() noexcept { return &_entry; }
    [[nodiscard]] auto entry_scope() const noexcept { return &_entry; }
};

}// namespace luisa::compute::xir
