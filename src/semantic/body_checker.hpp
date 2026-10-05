#pragma once
#include "ast/ast.hpp"
#include "diagnostic/diagnostic.hpp"
#include "semantic/scope_manager.hpp"
#include "semantic/type_resolver.hpp"
#include "semantic/body_semantics.hpp"
#include "semantic/coercion_checker.hpp"
#include "symbol.hpp"
#include "function_resolver.hpp"
#include "semantic_ids.hpp"
#include <optional>

namespace semantic {

class SemanticModel;
class DeriveChecker;

enum class LoopKind {
    Infinite,
    While
};

struct BreakSite {
    const ast::BreakExpression* expression;
    TyId value_type;
    bool reachable;
};

struct LoopFrame {
    LoopId id;
    LoopKind kind;
    std::optional<TyId> expected_result;
    std::vector<BreakSite> breaks;
};

struct ExprCheckResult {
    ExprSemantics semantics;
    bool can_complete;
};

struct BlockCheckResult {
    TyId type;
    bool can_complete;
};

struct StatementCheckResult {
    bool can_complete;
    bool has_error = false;
};

struct FunctionCheckContext {
    const FunctionInfo& function;
    ScopeManager scopes;
    FunctionBodyInfo body;
    std::vector<LoopFrame> loops;
    LoopId next_loop_id = 0;
    size_t loop_target_floor = 0;
    bool path_reachable = true;
    bool has_error = false;

    explicit FunctionCheckContext(const FunctionInfo& fn): function(fn) {}
};

class BodyChecker {
public:
    BodyChecker(const CrateIndex& index, SemanticModel& model, ConstEvaluator& constants, TypeResolver& type_resolver, DeriveChecker& derive_checker, diagnostic::DiagnosticCollector& diag);
    bool checkAll(const ast::Crate& crate);

private:
    const CrateIndex& index_;
    SemanticModel& model_;
    ConstEvaluator& constants_;
    TypeResolver& type_resolver_;
    DeriveChecker& derive_checker_;
    CoercionChecker coercions_;
    diagnostic::DiagnosticCollector& diag_;

    TyId error_type_;
    TyId unit_type_;
    TyId never_type_;
    TyId bool_type_;

    bool checkFunction(const FunctionInfo& function);
    LocalId addLocal(const std::string& name, TyId type, bool mutable_binding, bool is_parameter, ast::SourceSpan declaration, FunctionCheckContext& ctx);
    StatementCheckResult checkLet(const ast::LetStatement& statement, FunctionCheckContext& ctx);
    StatementCheckResult checkStatement(const ast::Statement& statement, FunctionCheckContext& ctx);
    ExprCheckResult checkExpr(const ast::Expression& expression, std::optional<TyId> expected, FunctionCheckContext& ctx);
    BlockCheckResult checkBlock(const ast::BlockExpression& expression, std::optional<TyId> expected, FunctionCheckContext& ctx);
    void report(FunctionCheckContext& ctx, ast::SourceSpan span, const std::string& message);
    bool applyCoercion(const ast::Expression& expr, const ExprSemantics& original, const CoercionPlan& plan, FunctionCheckContext& ctx);

    ExprCheckResult makeValue(TyId type, bool can_complete = true) const;
    ExprCheckResult makeError() const;
    ExprCheckResult checkExprRaw(const ast::Expression& expression, std::optional<TyId> expected, FunctionCheckContext& ctx);
    ExprCheckResult checkBlockRaw(const ast::BlockExpression& expression, std::optional<TyId> expected, FunctionCheckContext& ctx);
    ExprCheckResult checkGrouped(const ast::GroupedExpression& expression, std::optional<TyId> expected, FunctionCheckContext& ctx);
    ExprCheckResult checkUnimplemented(const ast::Expression& expression, const std::string& kind, FunctionCheckContext& ctx);
};

} // namespace semantic