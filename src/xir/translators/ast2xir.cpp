#include <luisa/xir/metadata/comment.h>
#include <luisa/core/logging.h>
#include <luisa/core/stl/unordered_map.h>
#include <luisa/ast/external_function.h>
#include <luisa/ast/statement.h>
#include <luisa/ast/function.h>
#include <luisa/xir/builder.h>
#include <luisa/xir/translators/ast2xir.h>

namespace luisa::compute::xir {

class AST2XIRContext {

public:
    struct BreakContinueTarget {
        BasicBlock *break_target{nullptr};
        BasicBlock *continue_target{nullptr};
    };

    struct Current {
        FunctionDefinition *f{nullptr};
        const ASTFunction *ast{nullptr};
        BreakContinueTarget break_continue_target;
        luisa::unordered_map<Variable, Value *> variables;
        luisa::vector<const CommentStmt *> comments;
    };

private:
    AST2XIRConfig _config;
    Module *_module;
    luisa::unordered_map<uint64_t, Function *> _generated_functions;
    luisa::unordered_map<uint64_t, Constant *> _generated_constants;
    luisa::unordered_map<uint64_t, Constant *> _generated_literals;
    Current _current;

private:
    Value *_translate_expression(Builder &b, const Expression *expr, bool load_lval) noexcept {
        return nullptr;
    }

    template<typename T>
    auto _commented(T inst) noexcept {
        for (auto comment : _current.comments) {
            inst->add_comment(comment->comment());
        }
        _current.comments.clear();
        return inst;
    }

    void _collect_comment(const Statement *stmt) noexcept {
        LUISA_ASSERT(stmt->tag() == Statement::Tag::COMMENT, "Unexpected statement type.");
        _current.comments.emplace_back(static_cast<const CommentStmt *>(stmt));
    }

    void _translate_switch_stmt(Builder &b, const SwitchStmt *ast_switch, luisa::span<const Statement *const> cdr) noexcept {
        // we do not support break/continue in switch statement
        auto old_break_continue_target = std::exchange(_current.break_continue_target, {});
        auto value = _translate_expression(b, ast_switch->expression(), true);
        auto inst = _commented(b.switch_(value));
        auto merge_block = inst->create_merge_block();
        auto case_break_removed = [](auto stmt_span) noexcept {
            while (!stmt_span.empty() &&
                   (stmt_span.back()->tag() == Statement::Tag::BREAK ||
                    stmt_span.back()->tag() == Statement::Tag::COMMENT)) {
                stmt_span = stmt_span.subspan(0, stmt_span.size() - 1);
            }
            return stmt_span;
        };
        for (auto s : ast_switch->body()->statements()) {
            switch (s->tag()) {
                case Statement::Tag::SWITCH_CASE: {
                    auto ast_case = static_cast<const SwitchCaseStmt *>(s);
                    LUISA_ASSERT(ast_case->expression()->tag() == Expression::Tag::LITERAL,
                                 "Unexpected switch case expression.");
                    auto ast_literal = static_cast<const LiteralExpr *>(ast_case->expression());
                    auto case_value = luisa::visit(
                        []<typename T>(T x) noexcept -> SwitchInst::case_value_type {
                            if constexpr (std::is_integral_v<T>) {
                                return static_cast<SwitchInst::case_value_type>(x);
                            } else {
                                LUISA_ERROR_WITH_LOCATION("Unexpected literal integer in switch case.");
                            }
                        },
                        ast_literal->value());
                    auto case_block = _commented(inst->create_case_block(case_value));
                    b.set_insertion_point(case_block);
                    auto case_stmts = case_break_removed(ast_case->body()->statements());
                    _translate_statements(b, case_stmts);
                    if (!b.is_insertion_point_terminator()) { b.br(merge_block); }
                    break;
                }
                case Statement::Tag::SWITCH_DEFAULT: {
                    LUISA_ASSERT(inst->default_block() == nullptr,
                                 "Multiple default blocks in a switch statement.");
                    auto default_block = inst->create_default_block();
                    b.set_insertion_point(default_block);
                    auto ast_default = static_cast<const SwitchDefaultStmt *>(s);
                    auto case_stmts = case_break_removed(ast_default->body()->statements());
                    _translate_statements(b, case_stmts);
                    if (!b.is_insertion_point_terminator()) { b.br(merge_block); }
                    break;
                }
                case Statement::Tag::COMMENT: {
                    _collect_comment(s);
                    break;
                }
                default: LUISA_ERROR_WITH_LOCATION("Unexpected statement in switch body.");
            }
        }
        if (inst->default_block() == nullptr) {
            b.set_insertion_point(inst->create_default_block());
            b.br(merge_block);
        }
        _current.break_continue_target = old_break_continue_target;
        b.set_insertion_point(merge_block);
        _translate_statements(b, cdr);
    }

    void _translate_if_stmt(Builder &b, const IfStmt *ast_if, luisa::span<const Statement *const> cdr) noexcept {
        auto cond = _translate_expression(b, ast_if->condition(), true);
        auto inst = _commented(b.if_(cond));
        auto merge_block = inst->create_merge_block();
        // true branch
        {
            b.set_insertion_point(inst->create_true_block());
            _translate_statements(b, ast_if->true_branch()->statements());
            if (!b.is_insertion_point_terminator()) { b.br(merge_block); }
        }
        // false branch
        {
            b.set_insertion_point(inst->create_false_block());
            _translate_statements(b, ast_if->false_branch()->statements());
            if (!b.is_insertion_point_terminator()) { b.br(merge_block); }
        }
        // merge block
        b.set_insertion_point(merge_block);
        _translate_statements(b, cdr);
    }

    void _translate_loop_stmt(Builder &b, const LoopStmt *ast_loop, luisa::span<const Statement *const> cdr) noexcept {
        auto inst = _commented(b.simple_loop());
        auto merge_block = inst->create_merge_block();
        auto body_block = inst->create_body_block();
        auto old_break_continue_target = std::exchange(_current.break_continue_target, {});
        // body block
        {
            _current.break_continue_target = {.break_target = merge_block,
                                              .continue_target = body_block};
            b.set_insertion_point(body_block);
            _translate_statements(b, ast_loop->body()->statements());
            if (!b.is_insertion_point_terminator()) { b.br(inst->body_block()); }
            _current.break_continue_target = {};
        }
        // merge block
        _current.break_continue_target = old_break_continue_target;
        b.set_insertion_point(merge_block);
        _translate_statements(b, cdr);
    }

    void _translate_for_stmt(Builder &b, const ForStmt *ast_for, luisa::span<const Statement *const> cdr) noexcept {
        auto var = _translate_expression(b, ast_for->variable(), false);
        auto inst = _commented(b.loop());
        auto merge_block = inst->create_merge_block();
        auto prepare_block = inst->create_prepare_block();
        auto body_block = inst->create_body_block();
        auto update_block = inst->create_update_block();
        auto old_break_continue_target = std::exchange(_current.break_continue_target, {});
        // prepare block
        {
            b.set_insertion_point(prepare_block);
            auto cond = _translate_expression(b, ast_for->condition(), true);
            b.cond_br(cond, body_block, merge_block);
        }
        // body block
        {
            _current.break_continue_target = {.break_target = merge_block,
                                              .continue_target = update_block};
            b.set_insertion_point(body_block);
            _translate_statements(b, ast_for->body()->statements());
            if (!b.is_insertion_point_terminator()) { b.br(update_block); }
            _current.break_continue_target = {};
        }
        // update block
        {
            b.set_insertion_point(update_block);
            auto t = ast_for->variable()->type();
            // var += step
            auto step = _translate_expression(b, ast_for->step(), true);
            auto cast_step = b.static_cast_if_necessary(t, step);
            auto prev = b.load(t, var);
            auto next = b.call(t, IntrinsicOp::BINARY_ADD, {prev, cast_step});
            b.store(var, next);
            // jump back to prepare block
            b.br(prepare_block);
        }
        // merge block
        _current.break_continue_target = old_break_continue_target;
        b.set_insertion_point(merge_block);
        _translate_statements(b, cdr);
    }

    void _translate_ray_query_stmt(Builder &b, const RayQueryStmt *ast_ray_query, luisa::span<const Statement *const> cdr) noexcept {
        // we do not support break/continue in ray query statement
        auto old_break_continue_target = std::exchange(_current.break_continue_target, {});
        auto query_object = _translate_expression(b, ast_ray_query->query(), false);
        auto inst = _commented(b.ray_query(query_object));
        auto merge_block = inst->create_merge_block();
        // on surface candidate block
        {
            b.set_insertion_point(inst->create_on_surface_candidate_block());
            _translate_statements(b, ast_ray_query->on_triangle_candidate()->statements());
            if (!b.is_insertion_point_terminator()) { b.br(merge_block); }
        }
        // on procedural candidate block
        {
            b.set_insertion_point(inst->create_on_procedural_candidate_block());
            _translate_statements(b, ast_ray_query->on_procedural_candidate()->statements());
            if (!b.is_insertion_point_terminator()) { b.br(merge_block); }
        }
        // merge block
        _current.break_continue_target = old_break_continue_target;
        b.set_insertion_point(merge_block);
        _translate_statements(b, cdr);
    }

    void _translate_statements(Builder &b, luisa::span<const Statement *const> stmts) noexcept {
        if (stmts.empty()) { return; }
        auto car = stmts.front();
        auto cdr = stmts.subspan(1);
        switch (car->tag()) {
            case Statement::Tag::BREAK: {
                auto break_target = _current.break_continue_target.break_target;
                LUISA_ASSERT(break_target != nullptr, "Invalid break statement.");
                _commented(b.break_(break_target));
                break;
            }
            case Statement::Tag::CONTINUE: {
                auto continue_target = _current.break_continue_target.continue_target;
                LUISA_ASSERT(continue_target != nullptr, "Invalid continue statement.");
                _commented(b.continue_(continue_target));
                break;
            }
            case Statement::Tag::RETURN: {
                if (auto ast_expr = static_cast<const ReturnStmt *>(car)->expression()) {
                    auto value = _translate_expression(b, ast_expr, true);
                    _commented(b.return_(value));
                } else {
                    _commented(b.return_void());
                }
                break;
            }
            case Statement::Tag::SCOPE: LUISA_ERROR_WITH_LOCATION("Unexpected scope statement.");
            case Statement::Tag::IF: {
                auto ast_if = static_cast<const IfStmt *>(car);
                _translate_if_stmt(b, ast_if, cdr);
                break;
            }
            case Statement::Tag::LOOP: {
                auto ast_loop = static_cast<const LoopStmt *>(car);
                _translate_loop_stmt(b, ast_loop, cdr);
                break;
            }
            case Statement::Tag::EXPR: {
                auto ast_expr = static_cast<const ExprStmt *>(car)->expression();
                _commented(_translate_expression(b, ast_expr, false));
                _translate_statements(b, cdr);
                break;
            }
            case Statement::Tag::SWITCH: {
                auto ast_switch = static_cast<const SwitchStmt *>(car);
                _translate_switch_stmt(b, ast_switch, cdr);
                break;
            }
            case Statement::Tag::SWITCH_CASE: LUISA_ERROR_WITH_LOCATION("Unexpected switch case statement.");
            case Statement::Tag::SWITCH_DEFAULT: LUISA_ERROR_WITH_LOCATION("Unexpected switch default statement.");
            case Statement::Tag::ASSIGN: {
                auto assign = static_cast<const AssignStmt *>(car);
                auto variable = _translate_expression(b, assign->lhs(), false);
                auto value = _translate_expression(b, assign->rhs(), true);
                _commented(b.store(variable, value));
                _translate_statements(b, cdr);
                break;
            }
            case Statement::Tag::FOR: {
                auto ast_for = static_cast<const ForStmt *>(car);
                _translate_for_stmt(b, ast_for, cdr);
                break;
            }
            case Statement::Tag::COMMENT: {
                _collect_comment(car);
                _translate_statements(b, cdr);
                break;
            }
            case Statement::Tag::RAY_QUERY: {
                auto ast_ray_query = static_cast<const RayQueryStmt *>(car);
                _translate_ray_query_stmt(b, ast_ray_query, cdr);
                break;
            }
            case Statement::Tag::AUTO_DIFF: LUISA_NOT_IMPLEMENTED();
            case Statement::Tag::PRINT: {
                auto ast_print = static_cast<const PrintStmt *>(car);
                luisa::fixed_vector<Value *, 16u> args;
                for (auto ast_arg : ast_print->arguments()) {
                    args.emplace_back(_translate_expression(b, ast_arg, true));
                }
                _commented(b.print(luisa::string{ast_print->format()}, args));
                _translate_statements(b, cdr);
                break;
            }
        }
    }

    void _translate_current_function() noexcept {
        // convert the arguments
        for (auto &&ast_arg : _current.ast->arguments()) {
            auto type = ast_arg.type();
            if (ast_arg.is_reference()) {
                _current.variables.emplace(ast_arg, _current.f->create_reference_argument(type));
            } else {
                _current.variables.emplace(ast_arg, _current.f->create_value_argument(type));
            }
        }
        // create the body block
        Builder b;
        b.set_insertion_point(_current.f->create_body_block());
        for (auto ast_local : _current.ast->local_variables()) {
            _current.variables.emplace(ast_local, b.alloca_local(ast_local.type()));
        }
        for (auto ast_shared : _current.ast->shared_variables()) {
            _current.variables.emplace(ast_shared, b.alloca_shared(ast_shared.type()));
        }
        _translate_statements(b, _current.ast->body()->statements());
        if (!b.is_insertion_point_terminator()) {
            LUISA_ASSERT(_current.f->type() == nullptr,
                         "Non-void function must have a return statement at the end.");
            b.return_void();
        }
    }

public:
    explicit AST2XIRContext(const AST2XIRConfig &config) noexcept
        : _config{config}, _module{Pool::current()->create<Module>()} {}

    Function *add_function(const ASTFunction &f) noexcept {
        // try emplace the function
        auto [iter, newly_added] = _generated_functions.try_emplace(f.hash(), nullptr);
        // return the function if it has been translated
        if (!newly_added) { return iter->second; }
        // create a new function
        FunctionDefinition *def = nullptr;
        switch (f.tag()) {
            case ASTFunction::Tag::KERNEL:
                def = _module->create_kernel();
                break;
            case ASTFunction::Tag::CALLABLE:
                def = _module->create_callable(f.return_type());
                break;
            case ASTFunction::Tag::RASTER_STAGE:
                LUISA_NOT_IMPLEMENTED();
        }
        iter->second = def;
        // translate the function
        auto old = std::exchange(_current, {.f = def, .ast = &f});
        _translate_current_function();
        _current = old;
        return def;
    }

    Function *add_external_function(const ASTExternalFunction &f) noexcept {
        LUISA_NOT_IMPLEMENTED();
    }

    [[nodiscard]] Module *finalize() noexcept {
        return _module;
    }
};

AST2XIRContext *ast_to_xir_translate_begin(const AST2XIRConfig &config) noexcept {
    return luisa::new_with_allocator<AST2XIRContext>(config);
}

void ast_to_xir_translate_add_function(AST2XIRContext *ctx, const ASTFunction &f) noexcept {
    ctx->add_function(f);
}

void ast_to_xir_translate_add_external_function(AST2XIRContext *ctx, const ASTExternalFunction &f) noexcept {
    ctx->add_external_function(f);
}

Module *ast_to_xir_translate_finalize(AST2XIRContext *ctx) noexcept {
    auto m = ctx->finalize();
    luisa::delete_with_allocator(ctx);
    return m;
}

}// namespace luisa::compute::xir
