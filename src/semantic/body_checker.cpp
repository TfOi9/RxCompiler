#include "body_checker.hpp"
#include "ast/ast.hpp"
#include "diagnostic/diagnostic.hpp"
#include "semantic/function_resolver.hpp"
#include "semantic/semantic_ids.hpp"
#include "semantic/type_resolver.hpp"
#include "semantic/types.hpp"
#include "semantic_model.hpp"
#include <cassert>
#include <optional>

namespace semantic {

BodyChecker::BodyChecker(const CrateIndex& index, SemanticModel& model, ConstEvaluator& constants, TypeResolver& type_resolver, diagnostic::DiagnosticCollector& diag):
    index_(index), model_(model), constants_(constants), type_resolver_(type_resolver), diag_(diag), error_type_(model.typeContext().error()), unit_type_(model.typeContext().insert(UnitTy{})), never_type_(model.typeContext().insert(NeverTy{})) {}

LocalId BodyChecker::addLocal(const std::string& name, TyId type, bool mutable_binding, bool is_parameter, ast::SourceSpan declaration, FunctionCheckContext& ctx) {
    LocalInfo local = {
        name,
        type,
        mutable_binding,
        is_parameter,
        ctx.scopes.depth(),
        declaration
    };
    const LocalId id = static_cast<LocalId>(ctx.body.locals.size());
    ctx.body.locals.push_back(local);
    ctx.scopes.bind(name, id);
    return id;
}

bool BodyChecker::checkFunction(const FunctionInfo& function) {
    if (function.kind == FunctionKind::Builtin) {
        return true;
    }
    if (!function.signature_valid) {
        return false;
    }
    assert(function.declaration != nullptr);
    assert(function.declaration->body != nullptr);
    FunctionCheckContext ctx(function);
    for (const auto& param: function.signature.parameters) {
        const auto id = addLocal(
            param.name,
            param.type,
            param.is_mut,
            true,
            param.span,
            ctx
        );
        ctx.body.parameter_locals.push_back(id);
    }
    const BlockCheckResult result = checkBlock(*function.declaration->body, function.signature.return_type, ctx);
    if (result.type == error_type_) {
        ctx.has_error = true;
    }
    const bool success = !ctx.has_error;
    model_.setFunctionBody(function.id, std::move(ctx.body));
    return success;
}

void BodyChecker::report(FunctionCheckContext& ctx, ast::SourceSpan span, const std::string& message) {
    ctx.has_error = true;
    diag_.add_entry(diagnostic::Severity::Error, span.begin, message);
}

bool BodyChecker::checkAll(const ast::Crate& crate) {
    bool success = true;
    auto checkDeclaration = [&](const ast::FunctionItem& decl) {
        const auto id = model_.findFunctionId(&decl);
        assert(id.has_value());
        const FunctionInfo* func = model_.findFunction(*id);
        assert(func != nullptr);
        const bool checked = checkFunction(*func);
        success = success && checked;
    };
    for (const auto& item: crate.items) {
        assert(item != nullptr);
        if (const auto* func = dynamic_cast<const ast::FunctionItem*>(item.get())) {
            checkDeclaration(*func);
            continue;
        }
        if (const auto* impl = dynamic_cast<const ast::ImplItem*>(item.get())) {
            for (const auto& assoc: impl->associated_items) {
                if (assoc.function.has_value() && assoc.function.value()) {
                    checkDeclaration(**assoc.function);
                }
            }
        }
    }
    return success;
}

StatementCheckResult BodyChecker::checkLet(const ast::LetStatement& statement, FunctionCheckContext& ctx) {
    assert(statement.expression != nullptr);
    std::optional<TyId> annotation;
    if (statement.type) {
        annotation = type_resolver_.resolve(*statement.type, ResolveContext{ctx.function.owner_struct});
        if (*annotation == error_type_) {
            ctx.has_error = true;
        }
    }
    const ExprCheckResult initializer = checkExpr(
        *statement.expression,
        annotation,
        ctx
    );
    if (initializer.semantics.type == error_type_) {
        ctx.has_error = true;
    }
    const TyId binding_type = annotation.has_value() ? *annotation : initializer.semantics.type;
    const auto& binding = statement.identifier_binding;
    const LocalId id = addLocal(
        binding.name,
        binding_type,
        binding.is_mut,
        false,
        binding.span,
        ctx
    );
    ctx.body.let_statements.emplace(&statement, id);
    return StatementCheckResult {
        initializer.can_complete
    };
}

StatementCheckResult BodyChecker::checkStatement(const ast::Statement& statement, FunctionCheckContext& ctx) {
    if (const auto* let = dynamic_cast<const ast::LetStatement*>(&statement)) {
        return checkLet(*let, ctx);
    }
    if (dynamic_cast<const ast::EmptyStatement*>(&statement)) {
        return {true};
    }
    if (const auto* expr = dynamic_cast<const ast::ExpressionStatement*>(&statement)) {
        assert(expr->expr != nullptr);
        const auto& expected = expr->has_semicolon ? std::nullopt : std::optional<TyId>(unit_type_);
        const ExprCheckResult result = checkExpr(*expr->expr, expected, ctx);
        return {result.can_complete};
    }
    report(ctx, statement.span, "unexpected statement type");
    return {true};
}

} // namespace semantic