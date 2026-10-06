#include "body_checker.hpp"
#include "ast/ast.hpp"
#include "diagnostic/diagnostic.hpp"
#include "semantic/body_semantics.hpp"
#include "semantic/checker_guards.hpp"
#include "semantic/coercion_checker.hpp"
#include "semantic/derive_checker.hpp"
#include "semantic/function_resolver.hpp"
#include "semantic/generic_arguments.hpp"
#include "semantic/impl_resolver.hpp"
#include "semantic/integer_literal.hpp"
#include "semantic/path_resolution.hpp"
#include "semantic/place_checker.hpp"
#include "semantic/scope_manager.hpp"
#include "semantic/semantic_ids.hpp"
#include "semantic/struct_resolver.hpp"
#include "semantic/symbol.hpp"
#include "semantic/type_resolver.hpp"
#include "semantic/types.hpp"
#include "semantic/const_evaluator.hpp"
#include "semantic_model.hpp"
#include <cassert>
#include <optional>
#include <variant>

namespace semantic {

BodyChecker::BodyChecker(const CrateIndex& index, SemanticModel& model, ConstEvaluator& constants, TypeResolver& type_resolver, DeriveChecker& derive_checker, diagnostic::DiagnosticCollector& diag):
    index_(index), model_(model), constants_(constants), type_resolver_(type_resolver), derive_checker_(derive_checker), coercions_(model.typeContext()), diag_(diag), error_type_(model.typeContext().error()), unit_type_(model.typeContext().insert(UnitTy{})), never_type_(model.typeContext().insert(NeverTy{})), bool_type_(model.typeContext().insert(PrimaryTy{PrimaryTyKind::Bool})) {}

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
    if (initializer.semantics.effectiveType() == error_type_) {
        ctx.has_error = true;
    }
    const TyId binding_type = annotation.has_value() ? *annotation : initializer.semantics.effectiveType();
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
    bool failed = (annotation.has_value() && *annotation == error_type_) || initializer.semantics.effectiveType() == error_type_;
    ctx.has_error = ctx.has_error || failed;
    return StatementCheckResult {
        initializer.can_complete,
        failed
    };
}

StatementCheckResult BodyChecker::checkStatement(const ast::Statement& statement, FunctionCheckContext& ctx) {
    if (const auto* let = dynamic_cast<const ast::LetStatement*>(&statement)) {
        return checkLet(*let, ctx);
    }
    if (dynamic_cast<const ast::EmptyStatement*>(&statement)) {
        return {true, false};
    }
    if (const auto* expr = dynamic_cast<const ast::ExpressionStatement*>(&statement)) {
        assert(expr->expr != nullptr);
        const auto& expected = expr->has_semicolon ? std::nullopt : std::optional<TyId>(unit_type_);
        const ExprCheckResult result = checkExpr(*expr->expr, expected, ctx);
        return {result.can_complete, result.semantics.effectiveType() == error_type_};
    }
    report(ctx, statement.span, "unexpected statement type");
    return {true, true};
}

bool BodyChecker::applyCoercion(const ast::Expression& expr, const ExprSemantics& original, const CoercionPlan& plan, FunctionCheckContext& ctx) {
    auto fail = [&](const std::string& message) {
        report(ctx, expr.span, message);
        ExprSemantics failed = original;
        failed.coerced_type = error_type_;
        ctx.body.coercions.erase(&expr);
        ctx.body.expressions.insert_or_assign(&expr, std::move(failed));
        return false;
    };
    if (plan.kind == CoercionKind::Recovery) {
        ctx.has_error = true;
        ExprSemantics recovered = original;
        recovered.coerced_type = error_type_;
        ctx.body.coercions.erase(&expr);
        ctx.body.expressions.insert_or_assign(&expr, std::move(recovered));
        return false;
    }
    if (plan.kind == CoercionKind::BorrowMutable) {
        assert(plan.reference_access.has_value());
        if (!canWrite(*plan.reference_access)) {
            return fail("coercion requires mutable access");
        }
    }
    ExprSemantics adjusted = original;
    adjusted.coerced_type = plan.target_type;
    ctx.body.coercions.insert_or_assign(&expr, plan);
    ctx.body.expressions.insert_or_assign(&expr, std::move(adjusted));
    return true;
}

ExprCheckResult BodyChecker::makeValue(TyId type, bool can_complete) const {
    ExprSemantics info;
    info.type = type;
    info.category = ValueCategory::Value;
    info.access = PlaceAccess::NotPlace;
    return ExprCheckResult{
        info,
        can_complete
    };
}

ExprCheckResult BodyChecker::makeError() const {
    return ExprCheckResult{
        ExprSemantics{error_type_},
        true
    };
}

ExprCheckResult BodyChecker::checkExpr(const ast::Expression& expression, std::optional<TyId> expected, FunctionCheckContext& ctx) {
    ExprCheckResult result = checkExprRaw(expression, expected, ctx);
    assert(!result.semantics.coerced_type.has_value());
    ctx.body.coercions.erase(&expression);
    ctx.body.expressions.insert_or_assign(&expression, result.semantics);
    if (result.semantics.type == error_type_) {
        ctx.has_error = true;
        return result;
    }
    if (!expected.has_value()) {
        return result;
    }
    auto plan = coercions_.tryCoerce(expression, result.semantics, *expected);
    if (!plan.has_value()) {
        report(ctx, expression.span, "expression type does not match expected");
        result.semantics.coerced_type = error_type_;
        ctx.body.expressions.insert_or_assign(&expression, result.semantics);
        return result;
    }
    applyCoercion(expression, result.semantics, *plan, ctx);
    result.semantics = ctx.body.expressions.at(&expression);
    return result;
}

ExprCheckResult BodyChecker::checkExprRaw(const ast::Expression& expression, std::optional<TyId> expected, FunctionCheckContext& ctx) {
    switch (expression.type) {
        case ast::NodeType::BoolExpr:
            return makeValue(bool_type_);
        case ast::NodeType::UnitExpr:
            return makeValue(unit_type_);
        case ast::NodeType::GroupedExpr:
            return checkGrouped(static_cast<const ast::GroupedExpression&>(expression), expected, ctx);
        case ast::NodeType::BlockExpr:
            return checkBlockRaw(static_cast<const ast::BlockExpression&>(expression), expected, ctx);
        case ast::NodeType::IntegerExpr:
            return checkInteger(static_cast<const ast::IntegerExpression&>(expression), expected, ctx);
        case ast::NodeType::PathExpr:
            return checkPath(static_cast<const ast::PathExpression&>(expression), ctx);
        case ast::NodeType::UnaryExpr:
            return checkUnimplemented(expression, "unary", ctx);
        case ast::NodeType::BinaryExpr:
            return checkUnimplemented(expression, "binary", ctx);
        case ast::NodeType::AssignExpr:
            return checkUnimplemented(expression, "assignment", ctx);
        case ast::NodeType::CastExpr:
            return checkUnimplemented(expression, "cast", ctx);
        case ast::NodeType::CallExpr:
            return checkUnimplemented(expression, "call", ctx);
        case ast::NodeType::MethodCallExpr:
            return checkUnimplemented(expression, "method call", ctx);
        case ast::NodeType::FieldExpr:
            return checkUnimplemented(expression, "field", ctx);
        case ast::NodeType::IndexExpr:
            return checkUnimplemented(expression, "index", ctx);
        case ast::NodeType::ArrayExpr:
            return checkUnimplemented(expression, "array", ctx);
        case ast::NodeType::StructExpr:
            return checkUnimplemented(expression, "struct", ctx);
        case ast::NodeType::IfExpr:
            return checkUnimplemented(expression, "if", ctx);
        case ast::NodeType::LoopExpr:
            return checkUnimplemented(expression, "loop", ctx);
        case ast::NodeType::WhileExpr:
            return checkUnimplemented(expression, "while", ctx);
        case ast:: NodeType::ReturnExpr:
            return checkUnimplemented(expression, "return", ctx);
        case ast::NodeType::BreakExpr:
            return checkUnimplemented(expression, "break", ctx);
        case ast::NodeType::ContinueExpr:
            return checkUnimplemented(expression, "continue", ctx);
        default:
            report(ctx, expression.span, "invalid expression node");
            return makeError();
    }
}

ExprCheckResult BodyChecker::checkBlockRaw(const ast::BlockExpression& expression, std::optional<TyId> expected, FunctionCheckContext& ctx) {
    ScopeGuard scope(ctx.scopes);
    bool can_complete = true;
    bool has_error = false;
    TyId tail_type = unit_type_;
    for (const auto& statement: expression.statements) {
        assert(statement != nullptr);
        StatementCheckResult result;
        {
            ReachabilityGuard reachable(ctx, can_complete);
            result = checkStatement(*statement, ctx);
        }
        has_error = has_error || result.has_error;
        can_complete = can_complete && result.can_complete;
    }
    if (expression.tail_expression) {
        ExprCheckResult tail;
        {
            ReachabilityGuard reachable(ctx, can_complete);
            tail = checkExpr(*expression.tail_expression, expected, ctx);
        }
        tail_type = tail.semantics.effectiveType();
        has_error = has_error || tail_type == error_type_;
        can_complete = can_complete && tail.can_complete;
    }
    if (has_error) {
        return makeValue(error_type_, can_complete);
    }
    if (!can_complete) {
        return makeValue(never_type_, can_complete);
    }
    return makeValue(tail_type, true);
}

BlockCheckResult BodyChecker::checkBlock(const ast::BlockExpression& expression, std::optional<TyId> expected, FunctionCheckContext& ctx) {
    const ExprCheckResult result = checkExpr(expression, expected, ctx);
    return BlockCheckResult{
        result.semantics.effectiveType(),
        result.can_complete
    };
}

ExprCheckResult BodyChecker::checkGrouped(const ast::GroupedExpression& expression, std::optional<TyId> expected, FunctionCheckContext& ctx) {
    assert(expression.expr != nullptr);
    ExprCheckResult inner = checkExpr(*expression.expr, expected, ctx);
    ExprSemantics info = inner.semantics;
    info.type = inner.semantics.effectiveType();
    info.coerced_type.reset();
    info.adjustments.clear();
    return ExprCheckResult{
        std::move(info),
        inner.can_complete
    };
}

ExprCheckResult BodyChecker::checkUnimplemented(const ast::Expression& expression, const std::string& kind, FunctionCheckContext& ctx) {
    report(ctx, expression.span, kind + " expression check is unimplemented");
    return makeError();
}

ExprCheckResult BodyChecker::checkInteger(const ast::IntegerExpression& expression, std::optional<TyId> expected, FunctionCheckContext& ctx) {
    const auto& literal = expression.value;
    const PrimaryTyKind kind = selectIntegerKind(literal.suffix, expectedIntegerKind(model_.typeContext(), expected));
    uint64_t magnitude = 0;
    std::string error;
    if (!parseIntegerMagnitude(literal, magnitude, error)) {
        report(ctx, expression.span, error);
        return makeError();
    }
    if (!checkPositiveIntegerMagnitude(kind, magnitude)) {
        report(ctx, literal.span, "integer literal is out of range");
        return makeError();
    }
    const TyId type = model_.typeContext().insert(PrimaryTy{kind});
    return makeValue(type);
}

std::optional<ResolvedValue> BodyChecker::resolveUnqualifiedValue(const ast::PathExprSegment& segment, FunctionCheckContext& ctx) {
    const auto& ident = segment.ident_segment;
    const auto* args = segment.generic_args ? &*segment.generic_args : nullptr;
    if (ident.is_Self) {
        report(ctx, segment.span, "'Self' is a type, not a value");
        return std::nullopt;
    }
    std::string name;
    if (ident.is_self) {
        if (ctx.function.signature.receiver_mode == ReceiverMode::None) {
            report(ctx, segment.span, "'self' is unavailable here");
            return std::nullopt;
        }
        name = "self";
    } else if (ident.name) {
        name = *ident.name;
    } else {
        report(ctx, segment.span, "expected a value name");
        return std::nullopt;
    }
    if (auto local = ctx.scopes.lookup(name)) {
        if (!checkNoGenericArguments(args, name, diag_)) {
            ctx.has_error = true;
            return std::nullopt;
        }
        return LocalTarget{*local};
    }
    auto found = index_.value_names.find(name);
    if (found == index_.value_names.end()) {
        report(ctx, segment.span, "unresolved value name " + name);
        return std::nullopt;
    }
    const Symbol& symbol = index_.symbols.at(found->second);
    return resolveTopLevelValue(symbol, segment, ctx);
}

std::optional<ResolvedValue> BodyChecker::resolveTopLevelValue(const Symbol& symbol, const ast::PathExprSegment& segment, FunctionCheckContext& ctx) {
    const auto* args = segment.generic_args ? &*segment.generic_args : nullptr;
    auto fail = [&](const std::string& message) -> std::optional<ResolvedValue> {
        report(ctx, segment.span, message);
        return std::nullopt;
    };
    switch (symbol.kind) {
        case SymbolKind::Constant: {
            if (!checkNoGenericArguments(args, symbol.name, diag_)) {
                ctx.has_error = true;
                return std::nullopt;
            }
            const auto* decl = dynamic_cast<const ast::ConstantItem*>(symbol.declaration);
            if (!decl) {
                return fail("constant symbol has no declaration");
            }
            auto id = constants_.findConstantId(decl);
            if (!id) {
                return fail("constant is not registered");
            }
            return ConstantTarget{*id};
        }
        case SymbolKind::Function:
        case SymbolKind::BuiltinValue: {
            const bool valid = symbol.kind == SymbolKind::Function ? checkLifetimeOnlyArguments(args, symbol.name, diag_) : checkNoGenericArguments(args, symbol.name, diag_);
            if (!valid) {
                ctx.has_error = true;
                return std::nullopt;
            }
            auto id = model_.findTopLevelFunction(symbol.id);
            if (!id) {
                return fail("function is not registered");
            }
            return FunctionTarget{*id};
        }
        default:
            return fail("name does not refer to a value");
    }
}

std::optional<ResolvedValue> BodyChecker::resolveAssociatedValue(const ast::PathExprSegment& owner, const ast::PathExprSegment& member, FunctionCheckContext& ctx) {
    auto fail = [&](const std::string& message) -> std::optional<ResolvedValue> {
        report(ctx, member.span, message);
        return std::nullopt;
    };
    const auto& member_ident = member.ident_segment;
    if (member_ident.is_self || member_ident.is_Self || !member_ident.name) {
        return fail("expected an associated item name");
    }
    const std::string& name = *member_ident.name;
    const auto* owner_args = owner.generic_args ? &*owner.generic_args : nullptr;
    const auto* member_args = member.generic_args ? &*member.generic_args : nullptr;
    const TyId owner_type = type_resolver_.resolveNamedType(owner.ident_segment, owner_args, owner.span, ResolveContext{ctx.function.owner_struct});
    if (owner_type == error_type_) {
        ctx.has_error = true;
        return std::nullopt;
    }
    const TyInfo owner_info = model_.typeContext().get(owner_type);
    if (const auto* structure = std::get_if<StructTy>(&owner_info)) {
        const AssocInfo* assoc = model_.findAssociated(structure->def, name);
        if (assoc != nullptr) {
            switch (assoc->kind) {
                case AssocKind::Constant: {
                    if (!checkNoGenericArguments(member_args, name, diag_)) {
                        ctx.has_error = true;
                        return std::nullopt;
                    }
                    if (assoc->const_decl == nullptr) {
                        return fail("associated constant has no declaration");
                    }
                    const auto id = constants_.findConstantId(assoc->const_decl);
                    if (!id) {
                        return fail("associated constant is not registered");
                    }
                    return ConstantTarget{*id};
                }
                case AssocKind::Function: {
                    if (!checkLifetimeOnlyArguments(member_args, name, diag_)) {
                        ctx.has_error = true;
                        return std::nullopt;
                    }
                    const auto id = model_.findAssociatedFunction(assoc->id);
                    if (!id) {
                        return fail("associated function is not registered");
                    }
                    return FunctionTarget{*id};
                }
            }
            return fail("unsupported associated item kind");
        }
    }
    std::optional<BuiltinOp> builtin;
    if (std::holds_alternative<BoxTy>(owner_info)) {
        if (name == "new") {
            builtin = BuiltinOp::BoxNew;
        }
    } else if (std::holds_alternative<VecTy>(owner_info)) {
        if (name == "new") {
            builtin = BuiltinOp::VecNew;
        } else if (name == "len") {
            builtin = BuiltinOp::VecLen;
        } else if (name == "is_empty") {
            builtin = BuiltinOp::VecIsEmpty;
        } else if (name == "push") {
            builtin = BuiltinOp::VecPush;
        } else if (name == "remove") {
            builtin = BuiltinOp::VecRemove;
        }
    }
    if (!builtin && name == "clone") {
        if (!derive_checker_.supports(owner_type, DeriveKind::Clone)) {
            return fail("type does not support builtin clone");
        }
        builtin = BuiltinOp::Clone;
    }
    if (!builtin) {
        return fail("type has no associated item named " + name);
    }
    if (!checkNoGenericArguments(member_args, name, diag_)) {
        ctx.has_error = true;
        return std::nullopt;
    }
    return BuiltinTarget{*builtin, owner_type};
}

std::optional<ResolvedValue> BodyChecker::resolveValuePath(const ast::PathInExpression& path, FunctionCheckContext& ctx) {
    if (path.segments.size() == 1) {
        return resolveUnqualifiedValue(path.segments.front(), ctx);
    } else if (path.segments.size() == 2) {
        return resolveAssociatedValue(path.segments[0], path.segments[1], ctx);
    }
    report(ctx, path.span, "unsupported value path");
    return std::nullopt;
}

ExprCheckResult BodyChecker::checkPath(const ast::PathExpression& expression, FunctionCheckContext& ctx) {
    auto resolved = resolveValuePath(expression.path, ctx);
    if (!resolved) {
        return makeError();
    }
    if (const auto* target = std::get_if<LocalTarget>(&*resolved)) {
        const LocalInfo& local = ctx.body.locals.at(target->id);
        ExprSemantics info;
        info.type = local.type;
        info.category = ValueCategory::Place;
        info.access = local.mutable_binding ? PlaceAccess::Writable : PlaceAccess::ReadOnly;
        info.place = PlaceInfo{
            local.mutable_binding,
            false,
            false,
        };
        info.local = target->id;
        return {
            std::move(info),
            true
        };
    }
    if (const auto* target = std::get_if<ConstantTarget>(&*resolved)) {
        const EvaluatedConst* value = constants_.findEvaluated(target->id);
        if (!value) {
            report(ctx, expression.span, "constant value is unavailable");
            return makeError();
        }
        ctx.body.constant_values.insert_or_assign(&expression, *value);
        return makeValue(value->type);
    }
    if (const auto* target = std::get_if<FunctionTarget>(&*resolved)) {
        report(ctx, expression.span, "function values are unsupported");
        return makeError();
    }
    if (const auto* target = std::get_if<BuiltinTarget>(&*resolved)) {
        report(ctx, expression.span, "builtin function values are unsupported");
        return makeError();
    }
    report(ctx, expression.span, "invalid resolved value target");
    return makeError();
}

} // namespace semantic