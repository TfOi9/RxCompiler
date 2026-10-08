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
#include "semantic/operator_checker.hpp"
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
#include <limits>
#include <optional>
#include <utility>
#include <variant>

namespace semantic {

BodyChecker::BodyChecker(const CrateIndex& index, SemanticModel& model, ConstEvaluator& constants, TypeResolver& type_resolver, DeriveChecker& derive_checker, diagnostic::DiagnosticCollector& diag):
    index_(index), model_(model), constants_(constants), type_resolver_(type_resolver), derive_checker_(derive_checker), coercions_(model.typeContext()), operators_(model_.typeContext(), derive_checker), diag_(diag), error_type_(model.typeContext().error()), unit_type_(model.typeContext().insert(UnitTy{})), never_type_(model.typeContext().insert(NeverTy{})), bool_type_(model.typeContext().insert(PrimaryTy{PrimaryTyKind::Bool})) {}

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
            return checkUnary(static_cast<const ast::UnaryExpression&>(expression), ctx);
        case ast::NodeType::BinaryExpr:
            return checkBinary(static_cast<const ast::BinaryExpression&>(expression), ctx);
        case ast::NodeType::AssignExpr:
            return checkAssignment(static_cast<const ast::AssignmentExpression&>(expression), ctx);
        case ast::NodeType::CastExpr:
            return checkCast(static_cast<const ast::CastExpression&>(expression), ctx);
        case ast::NodeType::CallExpr:
            return checkCall(static_cast<const ast::CallExpression&>(expression), ctx);
        case ast::NodeType::MethodCallExpr:
            return checkMethodCall(static_cast<const ast::MethodCallExpression&>(expression), ctx);
        case ast::NodeType::FieldExpr:
            return checkField(static_cast<const ast::FieldExpression&>(expression), ctx);
        case ast::NodeType::IndexExpr:
            return checkIndex(static_cast<const ast::IndexExpression&>(expression), ctx);
        case ast::NodeType::ArrayExpr:
            return checkArray(static_cast<const ast::ArrayExpression&>(expression), expected, ctx);
        case ast::NodeType::StructExpr:
            return checkStruct(static_cast<const ast::StructExpression&>(expression), ctx);
        case ast::NodeType::IfExpr:
            return checkIf(static_cast<const ast::IfExpression&>(expression), expected, ctx);
        case ast::NodeType::LoopExpr:
            return checkLoop(static_cast<const ast::LoopExpression&>(expression), expected, ctx);
        case ast::NodeType::WhileExpr:
            return checkWhile(static_cast<const ast::WhileExpression&>(expression), ctx);
        case ast:: NodeType::ReturnExpr:
            return checkReturn(static_cast<const ast::ReturnExpression&>(expression), ctx);
        case ast::NodeType::BreakExpr:
            return checkBreak(static_cast<const ast::BreakExpression&>(expression), ctx);
        case ast::NodeType::ContinueExpr:
            return checkContinue(static_cast<const ast::ContinueExpression&>(expression), ctx);
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

std::optional<ExprCheckResult> BodyChecker::checkNegatedLiteralOperand(const ast::Expression& expression, FunctionCheckContext& ctx) {
    auto rem = [&](ExprCheckResult result) -> std::optional<ExprCheckResult> {
        ctx.body.coercions.erase(&expression);
        ctx.body.expressions.insert_or_assign(&expression, result.semantics);
        return result;
    };
    if (expression.type == ast::NodeType::GroupedExpr) {
        const auto& grouped = static_cast<const ast::GroupedExpression&>(expression);
        assert(grouped.expr != nullptr);
        auto inner = checkNegatedLiteralOperand(*grouped.expr, ctx);
        if (!inner) {
            return std::nullopt;
        }
        return rem(std::move(*inner));
    }
    if (expression.type != ast::NodeType::IntegerExpr) {
        return std::nullopt;
    }
    const auto& integer = static_cast<const ast::IntegerExpression&>(expression);
    const auto& literal = integer.value;
    const PrimaryTyKind kind = selectIntegerKind(literal.suffix, std::nullopt);
    if (!isSignedKind(kind)) {
        report(ctx, expression.span, "unary negation requires a signed integer");
        return rem(makeValue(error_type_));
    }
    uint64_t magnitude = 0;
    std::string error;
    if (!parseIntegerMagnitude(literal, magnitude, error)) {
        report(ctx, expression.span, error);
        return rem(makeValue(error_type_));
    }
    if (!checkNegatedIntegerMagnitude(kind, magnitude)) {
        report(ctx, expression.span, "negative integer literal out of range");
        return rem(makeValue(error_type_));
    }
    const TyId type = model_.typeContext().insert(PrimaryTy{kind});
    return rem(makeValue(type));
}

ExprCheckResult BodyChecker::checkUnary(const ast::UnaryExpression& expression, FunctionCheckContext& ctx) {
    assert(expression.operand != nullptr);
    ctx.body.unary_operations.erase(&expression);
    ctx.body.place_operations.erase(&expression);
    ExprCheckResult operand;
    if (expression.op == ast::UnaryOperator::Negation) {
        auto literal = checkNegatedLiteralOperand(*expression.operand, ctx);
        if (literal) {
            operand = std::move(*literal);
        } else {
            operand = checkExpr(*expression.operand, std::nullopt, ctx);
        }
    } else {
        operand = checkExpr(*expression.operand, std::nullopt, ctx);
    }
    const TyId operand_type = operand.semantics.effectiveType();
    if (operand_type == error_type_) {
        return makeValue(error_type_, operand.can_complete);
    }
    switch (expression.op) {
        case ast::UnaryOperator::Negation:
        case ast::UnaryOperator::Not: {
            auto plan = operators_.checkScalarUnary(expression.op, operand_type);
            if (!plan) {
                report(ctx, expression.span, "unsupported unary operand type");
                return makeValue(error_type_, operand.can_complete);
            }
            ctx.body.unary_operations.insert_or_assign(&expression, *plan);
            return makeValue(plan->result_type, operand.can_complete);
        }
        case ast::UnaryOperator::Dereference: {
            PlaceChecker places(model_.typeContext());
            auto base = places.fromExpression(*expression.operand, operand.semantics);
            auto target = places.dereferenceOne(std::move(base));
            if (!target) {
                report(ctx, expression.span, "dereference requires a reference or Box");
                return makeValue(error_type_, operand.can_complete);
            }
            ExprSemantics info{};
            info.type = target->type;
            info.category = ValueCategory::Place;
            info.place = target->access;
            info.access = canWrite(target->access) ? PlaceAccess::Writable : PlaceAccess::ReadOnly;
            ctx.body.place_operations.insert_or_assign(&expression, PlacePlan{target->root, std::move(target->steps)});
            return ExprCheckResult{std::move(info), operand.can_complete};
        }
        case ast::UnaryOperator::Borrow:
        case ast::UnaryOperator::BorrowMut: {
            PlaceChecker places(model_.typeContext());
            auto place = places.fromExpression(*expression.operand, operand.semantics);
            place = places.materializeIfNeeded(std::move(place));
            const bool is_mut = expression.op == ast::UnaryOperator::BorrowMut;
            if (is_mut) {
                auto error = mutableAccessError(place);
                if (error) {
                    std::string message = "mutable borrow requires a writable place";
                    switch (*error) {
                        case MutableAccessError::NotPlace:
                            message = "mutable borrow requires a place";
                            break;
                        case MutableAccessError::SharedRef:
                            message = "cannot borrow through a shared reference";
                            break;
                        case MutableAccessError::VecAccess:
                            message = "mutable borrow is blocked by Vec access";
                            break;
                        case MutableAccessError::ImmutableStorage:
                            message = "cannot mutably borrow immutable storage";
                            break;
                    }
                    report(ctx, expression.span, message);
                    return makeValue(error_type_, operand.can_complete);
                }
            }
            const TyId reference_type = model_.typeContext().insert(RefTy{place.type, is_mut});
            ctx.body.place_operations.insert_or_assign(&expression, PlacePlan{place.root, std::move(place.steps)});
            return makeValue(reference_type, operand.can_complete);
        }
    }
    report(ctx, expression.span, "invalid unary operator");
    return makeValue(error_type_, operand.can_complete);
}

std::optional<bool> BodyChecker::knownBooleanLiteral(const ast::Expression& expression) {
    if (expression.type == ast::NodeType::BoolExpr) {
        return static_cast<const ast::BoolExpression&>(expression).value;
    }
    if (expression.type == ast::NodeType::GroupedExpr) {
        const auto& grouped = static_cast<const ast::GroupedExpression&>(expression);
        assert(grouped.expr != nullptr);
        return knownBooleanLiteral(*grouped.expr);
    }
    return std::nullopt;
}

ExprCheckResult BodyChecker::checkBinary(const ast::BinaryExpression& expression, FunctionCheckContext& ctx) {
    assert(expression.lhs_operand != nullptr);
    assert(expression.rhs_operand != nullptr);
    ctx.body.binary_operations.erase(&expression);
    const bool lazy = expression.op == ast::BinaryOperator::LogicalAnd || expression.op == ast::BinaryOperator::LogicalOr;
    const ExprCheckResult left = checkExpr(*expression.lhs_operand, std::nullopt, ctx);
    bool right_may_run = true;
    bool right_may_skip = false;
    if (lazy) {
        const auto known = knownBooleanLiteral(*expression.lhs_operand);
        if (known.has_value()) {
            const bool must_run_right = expression.op == ast::BinaryOperator::LogicalAnd ? *known : !*known;
            right_may_run = must_run_right;
            right_may_skip = !must_run_right;
        } else {
            right_may_run = true;
            right_may_skip = true;
        }
    }
    ExprCheckResult right;
    {
        ReachabilityGuard reachable(ctx, left.can_complete && right_may_run);
        right = checkExpr(*expression.rhs_operand, std::nullopt, ctx);
    }
    const bool can_complete = lazy ? left.can_complete && (right_may_skip || right.can_complete) : left.can_complete && right.can_complete;
    const TyId left_type = left.semantics.effectiveType();
    const TyId right_type = right.semantics.effectiveType();
    if (lazy) {
        bool failed = left_type == error_type_ || right_type == error_type_;
        if (left_type != error_type_ && left_type != bool_type_) {
            report(ctx, expression.lhs_operand->span, "left logical operand must be bool");
            failed = true;
        }
        if (right_type != error_type_ && right_type != bool_type_) {
            report(ctx, expression.rhs_operand->span, "right logical operand must be bool");
            failed = true;
        }
        if (failed) {
            return makeValue(error_type_, can_complete);
        }
        const BinaryPlan plan{
            OperandPlan{
                left_type,
                bool_type_,
                OperandReadKind::Value
            },
            OperandPlan{
                right_type,
                bool_type_,
                OperandReadKind::Value
            },
            bool_type_
        };
        ctx.body.binary_operations.insert_or_assign(&expression, plan);
        return makeValue(bool_type_, can_complete);
    }
    if (left_type == error_type_ || right_type == error_type_) {
        return makeValue(error_type_, can_complete);
    }
    std::optional<BinaryPlan> plan;
    switch (expression.op) {
        case ast::BinaryOperator::Add:
        case ast::BinaryOperator::Subtract:
        case ast::BinaryOperator::Multiply:
        case ast::BinaryOperator::Divide:
        case ast::BinaryOperator::Remainder:
            plan = operators_.checkArithmeticOperands(left_type, right_type);
            break;
        case ast::BinaryOperator::BitwiseAnd:
        case ast::BinaryOperator::BitwiseOr:
        case ast::BinaryOperator::BitwiseXor:
            plan = operators_.checkBitwiseOperands(left_type, right_type);
            break;
        case ast::BinaryOperator::ShiftLeft:
        case ast::BinaryOperator::ShiftRight:
            plan = operators_.checkShiftOperands(left_type, right_type);
            break;
        case ast::BinaryOperator::Less:
        case ast::BinaryOperator::LessEqual:
        case ast::BinaryOperator::Greater:
        case ast::BinaryOperator::GreaterEqual:
            plan = operators_.checkOrderingOperands(left_type, right_type);
            break;
        case ast::BinaryOperator::Equal:
        case ast::BinaryOperator::NotEqual:
            plan = operators_.checkEqualityOperands(left_type, right_type);
            break;
        default:
            report(ctx, expression.span, "invalid binary operator");
            return makeValue(error_type_, can_complete);
    }
    if (!plan.has_value()) {
        report(ctx, expression.span, "unsupported binary operand types");
        return makeValue(error_type_, can_complete);
    }
    ctx.body.binary_operations.insert_or_assign(&expression, *plan);
    return makeValue(plan->result_type, can_complete);
}

ExprCheckResult BodyChecker::checkCast(const ast::CastExpression& expression, FunctionCheckContext& ctx) {
    assert(expression.operand != nullptr);
    assert(expression.target_type != nullptr);
    const TyId target = type_resolver_.resolve(*expression.target_type, ResolveContext{ctx.function.owner_struct});
    const ExprCheckResult operand = checkExpr(*expression.operand, std::nullopt, ctx);
    const TyId source = operand.semantics.effectiveType();
    if (target == error_type_ || source == error_type_) {
        ctx.has_error = true;
        return makeValue(error_type_, operand.can_complete);
    }
    if (!isInteger(target, model_)) {
        report(ctx, expression.target_type->span, "cast target must be an integer type");
        return makeValue(error_type_, operand.can_complete);
    }
    if (!isInteger(source, model_) && source != bool_type_) {
        report(ctx, expression.operand->span, "cast operand must have integer or bool type");
        return makeValue(error_type_, operand.can_complete);
    }
    return makeValue(target, operand.can_complete);
}

bool BodyChecker::checkAssignmentDestination(const ast::Expression& expression, const ExprSemantics& semantics, FunctionCheckContext& ctx) {
    if (semantics.effectiveType() == error_type_) {
        return false;
    }
    PlaceChecker places(model_.typeContext());
    const PlaceResult destination = places.fromExpression(expression, semantics);
    const auto error = mutableAccessError(destination);
    if (!error) {
        return true;
    }
    std::string message;
    switch (*error) {
        case MutableAccessError::NotPlace:
            message = "assignment destination must be a place";
            break;
        case MutableAccessError::SharedRef:
            message = "cannot assign through a shared reference";
            break;
        case MutableAccessError::VecAccess:
            message = "assignment is blocked by Vec access";
            break;
        case MutableAccessError::ImmutableStorage:
            message = "cannot assign to immutable storage";
            break;
    }
    report(ctx, expression.span, message);
    return false;
}

std::optional<BinaryPlan> BodyChecker::checkCompoundAssignmentOperands(ast::AssignmentOperator op, TyId destination_type, TyId right_type) {
    switch (op) {
        case ast::AssignmentOperator::AssignAdd:
        case ast::AssignmentOperator::AssignSubtract:
        case ast::AssignmentOperator::AssignMultiply:
        case ast::AssignmentOperator::AssignDivide:
        case ast::AssignmentOperator::AssignRemainder:
            return operators_.checkArithmeticOperands(destination_type, right_type);
        case ast::AssignmentOperator::AssignBitwiseAnd:
        case ast::AssignmentOperator::AssignBitwiseOr:
        case ast::AssignmentOperator::AssignBitwiseXor:
            return operators_.checkBitwiseOperands(destination_type, right_type);
        case ast::AssignmentOperator::AssignShiftLeft:
        case ast::AssignmentOperator::AssignShiftRight:
            return operators_.checkShiftOperands(destination_type, right_type);
        case ast::AssignmentOperator::Assign:
            return std::nullopt;
    }
    return std::nullopt;
}

ExprCheckResult BodyChecker::checkAssignment(const ast::AssignmentExpression& expression, FunctionCheckContext& ctx) {
    assert(expression.lhs_operand != nullptr);
    assert(expression.rhs_operand != nullptr);
    const bool simple = expression.op == ast::AssignmentOperator::Assign;
    ctx.body.binary_operations.erase(&expression);
    const ExprCheckResult left = checkExpr(*expression.lhs_operand, std::nullopt, ctx);
    const TyId destination_type = left.semantics.effectiveType();
    bool destination_ok = checkAssignmentDestination(*expression.lhs_operand, left.semantics, ctx);
    if (!simple && destination_type != error_type_ && !std::holds_alternative<PrimaryTy>(model_.typeContext().get(destination_type))) {
        report(ctx, expression.lhs_operand->span, "compound assignment destination must have primitive type");
        destination_ok = false;
    }
    std::optional<TyId> right_expected;
    if (simple && destination_type != error_type_) {
        right_expected = destination_type;
    }
    const ExprCheckResult right = checkExpr(*expression.rhs_operand, right_expected, ctx);
    const TyId right_type = right.semantics.effectiveType();
    const bool can_complete = left.can_complete && right.can_complete;
    if (!destination_ok || right_type == error_type_) {
        ctx.has_error = true;
        return makeValue(error_type_, can_complete);
    }
    if (simple) {
        return makeValue(unit_type_, can_complete);
    }
    const auto plan = checkCompoundAssignmentOperands(expression.op, destination_type, right_type);
    if (!plan) {
        report(ctx, expression.span, "unsupported compound assignment operand types");
        return makeValue(error_type_, can_complete);
    }
    assert(plan->result_type == destination_type);
    ctx.body.binary_operations.insert_or_assign(&expression, *plan);
    return makeValue(unit_type_, can_complete);
}

ExprCheckResult BodyChecker::checkReturn(const ast::ReturnExpression& expression, FunctionCheckContext& ctx) {
    const TyId target = ctx.function.signature.return_type;
    if (expression.expr) {
        const ExprCheckResult value = checkExpr(*expression.expr, target, ctx);
        if (value.semantics.effectiveType() == error_type_) {
            return makeValue(error_type_, false);
        }
    } else if (target != unit_type_) {
        report(ctx, expression.span, "bare return requires a unit return type");
        return makeValue(error_type_, false);
    }
    return makeValue(never_type_, false);
}

ExprCheckResult BodyChecker::checkIf(const ast::IfExpression& expression, std::optional<TyId> expected, FunctionCheckContext& ctx) {
    const auto condition = checkExpr(*expression.condition, bool_type_, ctx);
    const bool has_else = expression.else_branch != nullptr;
    const auto branch_expected = has_else ? expected : std::optional<TyId>(unit_type_);
    ExprCheckResult then_result;
    {
        ReachabilityGuard guard(ctx, condition.can_complete);
        then_result = checkExpr(*expression.then_block, branch_expected, ctx);
    }
    ExprCheckResult else_result = makeValue(unit_type_);
    if (has_else) {
        ReachabilityGuard guard(ctx, condition.can_complete);
        else_result = checkExpr(*expression.else_branch, expected, ctx);
    }
    const bool can_complete = condition.can_complete && (then_result.can_complete || else_result.can_complete);
    if (condition.semantics.effectiveType() == error_type_ || then_result.semantics.effectiveType() == error_type_ || else_result.semantics.effectiveType() == error_type_) {
        return makeValue(error_type_, can_complete);
    }
    if (!has_else) {
        return makeValue(unit_type_, can_complete);
    }
    if (expected) {
        return makeValue(*expected, can_complete);
    }
    const std::vector<ResultSite> sites{
        ResultSite{
            expression.then_block.get(),
            then_result.semantics
        },
        ResultSite{
            expression.else_branch.get(),
            else_result.semantics
        }
    };
    auto lub = coercions_.tryFindCommonType(sites, never_type_);
    if (!lub) {
        report(ctx, expression.span, "if branches have no common result type");
        return makeValue(error_type_, can_complete);
    }
    bool ok = true;
    for (size_t i = 0; i < sites.size(); i++) {
        const bool applied = applyCoercion(*sites[i].expression, sites[i].original, lub->coversions[i], ctx);
        ok = ok && applied;
    }
    return makeValue(ok ? lub->target_type : error_type_, can_complete);
}

ExprCheckResult BodyChecker::checkBreak(const ast::BreakExpression& expression, FunctionCheckContext& ctx) {
    if (ctx.loops.size() <= ctx.loop_target_floor) {
        report(ctx, expression.span, "break has no permitted loop target");
        if (expression.expr) {
            checkExpr(*expression.expr, std::nullopt, ctx);
        }
        return makeValue(error_type_, false);
    }
    const size_t target_index = ctx.loops.size() - 1;
    const LoopId target_id = ctx.loops[target_index].id;
    const LoopKind target_kind = ctx.loops[target_index].kind;
    const auto target_expected = ctx.loops[target_index].expected_result;
    if (expression.expr && target_kind == LoopKind::While) {
        report(ctx, expression.span, "value-bearing break requires a loop target");
        checkExpr(*expression.expr, std::nullopt, ctx);
        return makeValue(error_type_, false);
    }
    ctx.body.break_targets.insert_or_assign(&expression, target_id);
    const size_t site_index = ctx.loops[target_index].breaks.size();
    ctx.loops[target_index].breaks.push_back(BreakSite{
        &expression,
        error_type_,
        false,
        false
    });
    TyId value_type = unit_type_;
    bool value_can_complete = true;
    if (expression.expr) {
        const auto value = checkExpr(*expression.expr, target_expected, ctx);
        value_type = value.semantics.effectiveType() == error_type_ ? error_type_ : value.semantics.type;
        value_can_complete = value.can_complete;
    } else if (target_expected && *target_expected != unit_type_) {
        report(ctx, expression.span, "unit break value does not match loop result");
        value_type = error_type_;
    }
    auto& site = ctx.loops[target_index].breaks[site_index];
    site.value_type = value_type;
    site.value_can_complete = value_can_complete;
    site.reachable = value_can_complete;
    return makeValue(value_type == error_type_ ? error_type_ : never_type_, false);
}

ExprCheckResult BodyChecker::checkContinue(const ast::ContinueExpression& expression, FunctionCheckContext& ctx) {
    if (ctx.loops.size() <= ctx.loop_target_floor) {
        report(ctx, expression.span, "continue has no permitted loop target");
        return makeValue(error_type_, false);
    }
    const LoopId target_id = ctx.loops.back().id;
    ctx.body.continue_targets.insert_or_assign(&expression, target_id);
    return makeValue(never_type_, false);
}

ExprCheckResult BodyChecker::checkLoop(const ast::LoopExpression& expression, std::optional<TyId> expected, FunctionCheckContext& ctx) {
    LoopFrame frame;
    BlockCheckResult body{error_type_, true};
    {
        LoopGuard guard(ctx, LoopKind::Infinite, expected);
        ctx.body.loop_ids.insert_or_assign(&expression, guard.id());
        body = checkBlock(*expression.body, unit_type_, ctx);
        frame = guard.takeFrame();
    }
    if (body.type == error_type_) {
        return makeValue(error_type_, true);
    }
    if (frame.breaks.empty()) {
        return makeValue(never_type_, false);
    }
    bool can_complete = false;
    bool all_never = true;
    for (const auto& site: frame.breaks) {
        if (site.value_type == error_type_) {
            return makeValue(error_type_, true);
        }
        can_complete = can_complete || site.value_can_complete;
        all_never = all_never && site.value_type == never_type_;
    }
    if (all_never) {
        return makeValue(never_type_, false);
    }
    if (expected) {
        return makeValue(*expected, can_complete);
    }
    std::vector<ResultSite> sites;
    sites.reserve(frame.breaks.size());
    for (const auto& site: frame.breaks) {
        const auto* operand = site.expression->expr.get();
        if (operand) {
            sites.push_back(ResultSite{
                operand,
                ctx.body.expressions.at(operand)
            });
        } else {
            sites.push_back(ResultSite{
                site.expression,
                makeValue(unit_type_).semantics
            });
        }
    }
    auto lub = coercions_.tryFindCommonType(sites, never_type_);
    if (!lub) {
        report(ctx, expression.span, "break values has no common result type");
        return makeValue(error_type_, can_complete);
    }
    bool ok = true;
    for (size_t i = 0; i < sites.size(); i++) {
        if (!frame.breaks[i].expression->expr) {
            continue;
        }
        const bool applied = applyCoercion(*sites[i].expression, sites[i].original, lub->coversions[i], ctx);
        ok = ok && applied;
    }
    return makeValue(ok ? lub->target_type : error_type_, can_complete);
}

ExprCheckResult BodyChecker::checkWhile(const ast::WhileExpression& expression, FunctionCheckContext& ctx) {
    LoopGuard guard(ctx, LoopKind::While, std::nullopt);
    ctx.body.loop_ids.insert_or_assign(&expression, guard.id());
    ExprCheckResult condition;
    {
        LoopTargetFloorGuard floor(ctx, ctx.loops.size());
        condition = checkExpr(*expression.condition, bool_type_, ctx);
    }
    const auto known = knownBooleanLiteral(*expression.condition);
    BlockCheckResult body;
    {
        ReachabilityGuard reachable(ctx, condition.can_complete && known.value_or(true));
        body = checkBlock(*expression.body, unit_type_, ctx);
    }
    const bool failed = condition.semantics.effectiveType() == error_type_ || body.type == error_type_;
    return makeValue(failed ? error_type_ : unit_type_, condition.can_complete);
}

ExprCheckResult BodyChecker::checkArray(const ast::ArrayExpression& expression, std::optional<TyId> expected, FunctionCheckContext& ctx) {
    std::optional<ArrayTy> target;
    if (expected) {
        const TyInfo info = model_.typeContext().get(*expected);
        if (const auto* array = std::get_if<ArrayTy>(&info)) {
            target = *array;
        }
    }
    const bool repeated = expression.repeated_length != nullptr;
    bool failed = false;
    uint64_t length = expression.elements.size();
    if (repeated) {
        assert(expression.elements.size() == 1);
        const TyId usize_type = model_.typeContext().insert(PrimaryTy{PrimaryTyKind::USize});
        const auto count = constants_.evaluate<uint64_t>(expression.repeated_length.get(), usize_type, ResolveContext{ctx.function.owner_struct});
        if (count) {
            length = *count;
        } else {
            failed = true;
        }
    }
    if (length > std::numeric_limits<uint32_t>::max()) {
        report(ctx, expression.span, "array length exceeds usize range");
        failed = true;
    }
    if (target && length != target->len) {
        report(ctx, expression.span, "array length mismatch");
        failed = true;
    }
    const std::optional<TyId> element_expected = target ? std::optional<TyId>(target->elem) : std::nullopt;
    std::vector<ResultSite> sites;
    bool can_complete = true;
    for (const auto& elem: expression.elements) {
        ExprCheckResult result;
        {
            ReachabilityGuard guard(ctx, can_complete);
            result = checkExpr(*elem, element_expected, ctx);
        }
        failed = failed || result.semantics.effectiveType() == error_type_;
        can_complete = can_complete && result.can_complete;
        if (!target) {
            sites.push_back({elem.get(), result.semantics});
        }
    }
    if (failed) {
        return makeValue(error_type_, can_complete);
    }
    TyId element_type;
    if (target) {
        element_type = target->elem;
    } else if (sites.empty()) {
        element_type = model_.typeContext().insert(PrimaryTy{PrimaryTyKind::I32});
    } else {
        auto lub = coercions_.tryFindCommonType(sites, never_type_);
        if (!lub) {
            report(ctx, expression.span, "array elements have no common type");
            return makeValue(error_type_, can_complete);
        }
        element_type = lub->target_type;
        bool ok = true;
        for (size_t i = 0; i < sites.size(); i++) {
            const bool applied = applyCoercion(*sites[i].expression, sites[i].original, lub->coversions[i], ctx);
            ok = ok && applied;
        }
        if (!ok) {
            return makeValue(error_type_, can_complete);
        }
    }
    if (repeated && length > 1 && !derive_checker_.supports(element_type, DeriveKind::Copy)) {
        report(ctx, expression.span, "array repetition requires Copy trait");
        return makeValue(error_type_, can_complete);
    }
    return makeValue(model_.typeContext().insert(ArrayTy{element_type, static_cast<uint32_t>(length)}), can_complete);
}

ExprCheckResult BodyChecker::checkStruct(const ast::StructExpression& expression, FunctionCheckContext& ctx) {
    TyId struct_type = error_type_;
    const StructInfo* info = nullptr;
    bool failed = false;
    if (expression.path.segments.size() != 1) {
        report(ctx, expression.path.span, "unsupported struct construction path");
        failed = true;
    } else {
        const auto& segment = expression.path.segments.front();
        const auto* args = segment.generic_args ? &*segment.generic_args : nullptr;
        struct_type = type_resolver_.resolveNamedType(segment.ident_segment, args, segment.span, ResolveContext{ctx.function.owner_struct});
        if (struct_type == error_type_) {
            failed = true;
        } else {
            const TyInfo type = model_.typeContext().get(struct_type);
            if (const auto* structure = std::get_if<StructTy>(&type)) {
                info = model_.findStruct(structure->def);
            }
            if (!info) {
                report(ctx, expression.path.span, "construction requires a name-field struct");
                failed = true;
            }
        }
    }
    std::vector<bool> seen(info ? info->fields.size() : 0, false);
    bool can_complete = true;
    for (const auto& initializer: expression.fields) {
        std::optional<TyId> field_expected;
        if (info) {
            const auto found = info->field_name.find(initializer.name);
            if (found == info->field_name.end()) {
                report(ctx, initializer.span, "unknown initializer field");
                failed = true;
            } else {
                const size_t ordinal = found->second;
                if (seen[ordinal]) {
                    report(ctx, initializer.span, "duplicated initializer field");
                    failed = true;
                }
                seen[ordinal] = true;
                field_expected = info->fields[ordinal].type;
            }
        }
        ExprCheckResult value;
        {
            ReachabilityGuard guard(ctx, can_complete);
            value = checkExpr(*initializer.value, field_expected, ctx);
        }
        failed = failed || value.semantics.effectiveType() == error_type_;
        can_complete = can_complete && value.can_complete;
    }
    if (info) {
        for (const auto& field: info->fields) {
            if (!seen[field.ordinal]) {
                report(ctx, expression.span, "missing initializer field " + field.name);
                failed = true;
            }
        }
    }
    return makeValue(failed ? error_type_ : struct_type, can_complete);
}

ExprCheckResult BodyChecker::checkIndex(const ast::IndexExpression& expression, FunctionCheckContext& ctx) {
    const auto base = checkExpr(*expression.base, std::nullopt, ctx);
    const TyId usize_type = model_.typeContext().insert(PrimaryTy{PrimaryTyKind::USize});
    ExprCheckResult index;
    {
        ReachabilityGuard guard(ctx, base.can_complete);
        index = checkExpr(*expression.index, usize_type, ctx);
    }
    const bool can_complete = base.can_complete && index.can_complete;
    if (base.semantics.effectiveType() == error_type_ || index.semantics.effectiveType() == error_type_) {
        return makeValue(error_type_, can_complete);
    }
    PlaceChecker places(model_.typeContext());
    auto cursor = places.fromExpression(*expression.base, base.semantics);
    while (true) {
        const TyInfo type = model_.typeContext().get(cursor.type);
        if (std::holds_alternative<ArrayTy>(type) || std::holds_alternative<VecTy>(type)) {
            break;
        }
        auto next = places.dereferenceOne(std::move(cursor));
        if (!next) {
            report(ctx, expression.base->span, "index base is not an array or Vec");
            return makeValue(error_type_, can_complete);
        }
        cursor = std::move(*next);
    }
    auto element = places.projectIndex(std::move(cursor), *expression.index);
    assert(element.has_value());
    ExprSemantics info;
    info.type = element->type;
    info.category = ValueCategory::Place;
    info.place = element->access;
    info.access = canWrite(element->access) ? PlaceAccess::Writable : PlaceAccess::ReadOnly;
    ctx.body.place_operations.insert_or_assign(&expression, PlacePlan{element->root, std::move(element->steps)});
    return {std::move(info), can_complete};
}

ExprCheckResult BodyChecker::checkField(const ast::FieldExpression& expression, FunctionCheckContext& ctx) {
    const auto base = checkExpr(*expression.base, std::nullopt, ctx);
    if (base.semantics.effectiveType() == error_type_) {
        return makeValue(error_type_, base.can_complete);
    }
    PlaceChecker places(model_.typeContext());
    auto cursor = places.fromExpression(*expression.base, base.semantics);
    while (true) {
        const TyInfo type = model_.typeContext().get(cursor.type);
        if (const auto* structure = std::get_if<StructTy>(&type)) {
            const FieldInfo* field = model_.findField(structure->def, expression.field_name);
            if (!field) {
                report(ctx, expression.span, "unknown accessed field");
                return makeValue(error_type_, base.can_complete);
            }
            auto projected = places.projectField(std::move(cursor), field->type, field->ordinal);
            ExprSemantics info;
            info.type = projected.type;
            info.category = ValueCategory::Place;
            info.place = projected.access;
            info.access = canWrite(projected.access) ? PlaceAccess::Writable : PlaceAccess::ReadOnly;
            info.field_ordinal = field->ordinal;
            ctx.body.place_operations.insert_or_assign(&expression, PlacePlan{projected.root, std::move(projected.steps)});
            return {std::move(info), base.can_complete};
        }
        auto next = places.dereferenceOne(std::move(cursor));
        if (!next) {
            report(ctx, expression.span, "field base is not a struct");
            return makeValue(error_type_, base.can_complete);
        }
        cursor = std::move(*next);
    }
}

Callable BodyChecker::describeFunction(FunctionId id) const {
    const FunctionInfo* function = model_.findFunction(id);
    assert(function != nullptr);
    assert(function->signature_valid);
    Callable result{
        FunctionTarget{id},
        {},
        function->signature.return_type,
        function->signature.receiver_mode
    };
    for (const auto& param: function->signature.parameters) {
        result.parameters.push_back(param.type);
    }
    return result;
}

ArgumentCheckResult BodyChecker::checkArguments(const std::vector<ast::AstPtr<ast::Expression>>& arguments, const std::vector<TyId>* expected_types, bool prefix_can_complete, ast::SourceSpan span, FunctionCheckContext& ctx) {
    bool failed = false;
    bool can_complete = prefix_can_complete;
    if (expected_types && arguments.size() != expected_types->size()) {
        report(ctx, span, "argument count mismatch");
        failed = true;
    }
    for (size_t i = 0; i < arguments.size(); i++) {
        std::optional<TyId> expected;
        if (expected_types && i < expected_types->size()) {
            expected = (*expected_types)[i];
        }
        ExprCheckResult argument;
        {
            ReachabilityGuard guard(ctx, can_complete);
            argument = checkExpr(*arguments[i], expected, ctx);
        }
        failed = failed || argument.semantics.effectiveType() == error_type_;
        can_complete = can_complete && argument.can_complete;
    }
    return {can_complete, failed};
}

ExprCheckResult BodyChecker::checkCall(const ast::CallExpression& expression, FunctionCheckContext& ctx) {
    const ast::Expression* callee = expression.callee.get();
    while (callee->type == ast::NodeType::GroupedExpr) {
        callee = static_cast<const ast::GroupedExpression*>(callee)->expr.get();
    }
    std::optional<Callable> callable;
    bool prefix_can_complete = true;
    if (callee->type == ast::NodeType::PathExpr) {
        const auto& path = static_cast<const ast::PathExpression&>(*callee);
        const auto resolved = resolveValuePath(path.path, ctx);
        if (resolved) {
            if (const auto* function = std::get_if<FunctionTarget>(&*resolved)) {
                callable = describeFunction(function->id);
            } else if (const auto* builtin = std::get_if<BuiltinTarget>(&*resolved)) {
                callable = describeBuiltin(*builtin, expression.span, ctx);
            } else {
                report(ctx, callee->span, "call target is not a function");
            }
        }
    } else {
        const auto checked = checkExpr(*callee, std::nullopt, ctx);
        prefix_can_complete = checked.can_complete;
        if (checked.semantics.effectiveType() != error_type_) {
            report(ctx, callee->span, "unsupported call target expression");
        }
    }
    const auto arguments = checkArguments(expression.args, callable ? &callable->parameters : nullptr, prefix_can_complete, expression.span, ctx);
    if (!callable || arguments.has_error) {
        return makeValue(error_type_, arguments.can_complete);
    }
    ctx.body.calls.insert_or_assign(&expression, CallPlan{callable->target, std::nullopt});
    return makeValue(callable->return_type, arguments.can_complete);
}

std::optional<ReceiverPlan> BodyChecker::adjustReceiver(const ast::Expression& expression, TyId original_type, const ReceiverCandidate& candidate, FunctionCheckContext& ctx) {
    PlaceChecker places(model_.typeContext());
    PlaceResult place = candidate.source;
    ReceiverAction action = candidate.action;
    if (action == ReceiverAction::Value) {
        const TyInfo info = model_.typeContext().get(place.type);
        if (const auto* ref = std::get_if<RefTy>(&info); ref && ref->is_mut) {
            auto referent = places.dereferenceOne(std::move(place));
            assert(referent.has_value());
            place = std::move(*referent);
            action = ReceiverAction::BorrowMutable;
        }
    } else {
        place = places.materializeIfNeeded(std::move(place));
    }
    if (action == ReceiverAction::BorrowMutable && !canWrite(place.access)) {
        report(ctx, expression.span, "method receiver requires mutable access");
        return std::nullopt;
    }
    return ReceiverPlan{
        &expression,
        original_type,
        candidate.type,
        std::move(place.steps),
        action
    };
}

std::optional<Callable> BodyChecker::describeBuiltin(const BuiltinTarget& target, ast::SourceSpan span, FunctionCheckContext& ctx) {
    auto& types = model_.typeContext();
    const TyId owner = target.owner_type;
    const TyInfo owner_info = types.get(owner);
    if (std::holds_alternative<ErrorTy>(owner_info)) {
        ctx.has_error = true;
        return std::nullopt;
    }
    auto fail = [&](const std::string& message) -> std::optional<Callable> {
        report(ctx, span, message);
        return std::nullopt;
    };
    auto make = [&](std::vector<TyId> parameters, TyId return_type, ReceiverMode receiver_mode) -> std::optional<Callable> {
        return Callable{
            CallTarget{target},
            std::move(parameters),
            return_type,
            receiver_mode
        };
    };
    auto reference = [&](bool is_mut) -> TyId {
        return types.insert(TyInfo{RefTy{owner, is_mut}});
    };
    auto usizeType = [&]() -> TyId {
        return types.insert(TyInfo{PrimaryTy{PrimaryTyKind::USize}});
    };
    switch (target.operation) {
        case BuiltinOp::BoxNew: {
            const auto* box = std::get_if<BoxTy>(&owner_info);
            if (!box) {
                return fail("builtin Box::new requires a box type");
            }
            return make({box->elem}, owner, ReceiverMode::None);
        }
        case BuiltinOp::VecNew: {
            if (!std::holds_alternative<VecTy>(owner_info)) {
                return fail("builtin Vec::new requires a Vec type");
            }
            return make({}, owner, ReceiverMode::None);
        }
        case BuiltinOp::VecLen: {
            if (!std::holds_alternative<VecTy>(owner_info)) {
                return fail("builtin Vec::len requires a Vec type");
            }
            return make({reference(false)}, usizeType(), ReceiverMode::Ref);
        }
        case BuiltinOp::VecIsEmpty: {
            if (!std::holds_alternative<VecTy>(owner_info)) {
                return fail("builtin Vec::is_empty requires a Vec type");
            }
            return make({reference(false)}, bool_type_, ReceiverMode::Ref);
        }
        case BuiltinOp::VecPush: {
            const auto* vec = std::get_if<VecTy>(&owner_info);
            if (!vec) {
                return fail("builtin Vec::push requires a Vec type");
            }
            return make({reference(true), vec->elem}, unit_type_, ReceiverMode::MutableRef);
        }
        case BuiltinOp::VecRemove: {
            const auto* vec = std::get_if<VecTy>(&owner_info);
            if (!vec) {
                return fail("builtin Vec::remove requires a Vec type");
            }
            return make({reference(true), usizeType()}, vec->elem, ReceiverMode::MutableRef);
        }
        case BuiltinOp::Clone: {
            if (!derive_checker_.supports(owner, DeriveKind::Clone)) {
                return fail("type does not support builtin Clone");
            }
            return make({reference(false)}, owner, ReceiverMode::Ref);
        }
        case semantic::BuiltinOp::ArrayLen: {
            if (!std::holds_alternative<ArrayTy>(owner_info)) {
                return fail("builtin Array::len requires an Array type");
            }
            return make({reference(false)}, usizeType(), ReceiverMode::Ref);
        }
    }
}

std::optional<BuiltinOp> BodyChecker::findBuiltinOperation(TyId owner, const std::string& name) {
    const TyInfo info = model_.typeContext().get(owner);
    if (std::holds_alternative<BoxTy>(info)) {
        if (name == "new") {
            return BuiltinOp::BoxNew;
        }
    } else if (std::holds_alternative<VecTy>(info)) {
        if (name == "new") {
            return BuiltinOp::VecNew;
        }
        if (name == "len") {
            return BuiltinOp::VecLen;
        }
        if (name == "is_empty") {
            return BuiltinOp::VecIsEmpty;
        }
        if (name == "push") {
            return BuiltinOp::VecPush;
        }
        if (name == "remove") {
            return BuiltinOp::VecRemove;
        }
    } else if (std::holds_alternative<ArrayTy>(info)) {
        if (name == "len") {
            return BuiltinOp::ArrayLen;
        }
    }
    if (name == "clone" &&
        derive_checker_.supports(owner, DeriveKind::Clone)) {
        return BuiltinOp::Clone;
    }
    return std::nullopt;
}

std::vector<Callable> BodyChecker::collectBuiltinMethods(TyId candidate_type, const std::string& name, ast::SourceSpan span, FunctionCheckContext& ctx) {
    std::vector<Callable> result;
    const TyInfo candidate_info = model_.typeContext().get(candidate_type);
    const auto* reference = std::get_if<RefTy>(&candidate_info);
    if (!reference) {
        return result;
    }
    const TyId owner = reference->target;
    const auto operation = findBuiltinOperation(owner, name);
    if (!operation) {
        return result;
    }
    auto callable = describeBuiltin(
        BuiltinTarget{*operation, owner},
        span,
        ctx
    );
    if (!callable ||
        callable->receiver_mode == ReceiverMode::None ||
        callable->parameters.empty()) {
        return result;
    }
    if (callable->parameters.front() != candidate_type) {
        return result;
    }
    result.push_back(std::move(*callable));
    return result;
}

std::vector<Callable> BodyChecker::lookupMethods(TyId candidate_type, const std::string& name, ast::SourceSpan span, FunctionCheckContext& ctx) {
    std::vector<Callable> result;
    const TyInfo candidate_info = model_.typeContext().get(candidate_type);
    TyId owner = candidate_type;
    if (const auto* reference = std::get_if<RefTy>(&candidate_info)) {
        owner = reference->target;
    }
    const TyInfo owner_info = model_.typeContext().get(owner);
    if (const auto* structure = std::get_if<StructTy>(&owner_info)) {
        const AssocInfo* assoc = model_.findAssociated(structure->def, name);
        if (assoc && assoc->kind == AssocKind::Function) {
            const auto id = model_.findAssociatedFunction(assoc->id);
            assert(id.has_value());
            auto callable = describeFunction(*id);
            if (callable.receiver_mode != ReceiverMode::None &&
                !callable.parameters.empty() &&
                callable.parameters.front() == candidate_type) {
                result.push_back(std::move(callable));
            }
        }
    }
    auto builtins = collectBuiltinMethods(candidate_type, name, span, ctx);
    for (auto& callable: builtins) {
        result.push_back(std::move(callable));
    }
    return result;
}

std::optional<MethodSelection> BodyChecker::findMethod(const ast::Expression& expression, const ExprSemantics& semantics, const std::string& name, ast::SourceSpan span, FunctionCheckContext& ctx) {
    PlaceChecker places(model_.typeContext());
    auto cursor = places.fromExpression(expression, semantics);
    while (true) {
        const TyId direct = cursor.type;
        const TyId shared = model_.typeContext().insert(RefTy{direct, false});
        const TyId mutable_ref = model_.typeContext().insert(RefTy{direct, true});
        const ReceiverCandidate candidates[] = {
            {direct, cursor, ReceiverAction::Value},
            {shared, cursor, ReceiverAction::BorrowShared},
            {mutable_ref, cursor, ReceiverAction::BorrowMutable}
        };
        for (const auto& cand: candidates) {
            auto matches = lookupMethods(cand.type, name, span, ctx);
            if (matches.size() > 1) {
                report(ctx, span, "competing method candidates");
                return std::nullopt;
            }
            if (matches.size() == 1) {
                return MethodSelection{std::move(matches.front()), cand};
            }
        }
        auto next = places.dereferenceOne(std::move(cursor));
        if (!next) break;
        cursor = std::move(*next);
    }
    report(ctx, span, "no matching method");
    return std::nullopt;
}

ExprCheckResult BodyChecker::checkMethodCall(const ast::MethodCallExpression& expression, FunctionCheckContext& ctx) {
    const auto receiver = checkExpr(*expression.receiver, std::nullopt, ctx);
    const auto& ident = expression.method.ident_segment;
    const auto* generic_args = expression.method.generic_args ? &*expression.method.generic_args : nullptr;
    bool valid_name = ident.name.has_value() && !ident.is_Self && !ident.is_self;
    if (!valid_name) {
        report(ctx, expression.method.span, "expected a method name");
    } else if (!checkLifetimeOnlyArguments(generic_args, *ident.name, diag_)) {
        ctx.has_error = true;
        valid_name = false;
    }
    std::optional<MethodSelection> selected;
    if (valid_name && receiver.semantics.effectiveType() != error_type_) {
        selected = findMethod(*expression.receiver, receiver.semantics, *ident.name, expression.method.span, ctx);
    }
    std::optional<ReceiverPlan> receiver_plan;
    std::vector<TyId> ordinary_parameters;
    if (selected) {
        assert(selected->callable.receiver_mode != ReceiverMode::None);
        assert(!selected->callable.parameters.empty());
        receiver_plan = adjustReceiver(*expression.receiver, receiver.semantics.effectiveType(), selected->receiver, ctx);
        ordinary_parameters.assign(selected->callable.parameters.begin() + 1, selected->callable.parameters.end());
    }
    const auto arguments = checkArguments(expression.args, selected ? &ordinary_parameters : nullptr, receiver.can_complete, expression.span, ctx);
    if (!selected || !receiver_plan || arguments.has_error) {
        return makeValue(error_type_, arguments.can_complete);
    }
    ctx.body.calls.insert_or_assign(&expression, CallPlan{selected->callable.target, std::move(receiver_plan)});
    return makeValue(selected->callable.return_type, arguments.can_complete);
}

} // namespace semantic

