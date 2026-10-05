#include "const_evaluator.hpp"
#include "ast/ast.hpp"
#include "diagnostic/diagnostic.hpp"
#include "semantic/symbol.hpp"
#include "semantic/type_resolver.hpp"
#include "semantic/types.hpp"
#include "semantic/semantic_model.hpp"
#include "semantic/integer_literal.hpp"
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace semantic {

void ConstEvaluator::collectDefinitions() {
    definitions_.clear();
    id_decl_.clear();
    for (const auto& sym: index_.symbols) {
        if (sym.kind != SymbolKind::Constant) {
            continue;
        }
        const auto* decl = dynamic_cast<const ast::ConstantItem*>(sym.declaration);
        ConstId id = definitions_.size();
        ConstDef constant = {
            decl,
            std::nullopt,
            0
        };
        definitions_.push_back(constant);
        id_decl_[decl] = id;
    }
    for (const auto& assoc: model_.assocs_) {
        if (assoc.kind != AssocKind::Constant) {
            continue;
        }
        const auto* decl = assoc.const_decl;
        ConstId id = definitions_.size();
        ConstDef constant = {
            decl,
            assoc.owner,
            0
        };
        definitions_.push_back(constant);
        id_decl_[decl] = id;
    }
}

void ConstEvaluator::resolveTypes(TypeResolver& type_resolver) {
    for (auto& def: definitions_) {
        def.type_resolved = false;
        const auto* decl = def.declaration;
        if (decl == nullptr) {
            diag_.add_entry(diagnostic::Severity::Error, ast::SourceLocation {0, 0, 0}, "constant definition has no declaration");
            continue;
        }
        if (decl->type == nullptr) {
            diag_.add_entry(diagnostic::Severity::Error, decl->span.begin, "constant has no declared type");
            continue;
        }
        TyId type = type_resolver.resolve(*decl->type, ResolveContext {def.owner});
        const auto& info = model_.typeContext().get(type);
        if (std::holds_alternative<ErrorTy>(info)) {
            diag_.add_entry(diagnostic::Severity::Error, decl->span.begin, "type is error type");
            continue;
        }
        def.type = type;
        def.type_resolved = true;
    }
}

std::optional<ConstId> ConstEvaluator::resolvePath(const ast::PathInExpression& path, ResolveContext ctx) {
    if (path.segments.empty()) {
        diag_.add_entry(diagnostic::Severity::Error, path.span.begin, "empty constant path");
        return std::nullopt;
    }
    if (path.segments.size() == 1) {
        return resolveTopLevelConstant(path.segments[0]);
    }
    if (path.segments.size() == 2) {
        auto owner = resolveStructPrefix(path.segments[0], ctx);
        if (!owner) {
            return std::nullopt;
        }
        return resolveAssociatedConstant(*owner, path.segments[1]);
    }
    diag_.add_entry(diagnostic::Severity::Error, path.span.begin, "unsupported constant path");
    return std::nullopt;
}

std::optional<ConstId> ConstEvaluator::resolveTopLevelConstant(const ast::PathExprSegment& seg) {
    if (!seg.ident_segment.name.has_value()) {
        diag_.add_entry(diagnostic::Severity::Error, seg.span.begin, "constant path has empty name");
        return std::nullopt;
    }
    auto it = index_.value_names.find(*seg.ident_segment.name);
    if (it == index_.value_names.end()) {
        diag_.add_entry(diagnostic::Severity::Error, seg.span.begin, "unresolved constant");
        return std::nullopt;
    }
    const auto& sym = index_.symbols.at(it->second);
    if (sym.kind != SymbolKind::Constant) {
        diag_.add_entry(diagnostic::Severity::Error, seg.span.begin, "path does not name a constant");
        return std::nullopt;
    }
    const auto* decl = dynamic_cast<const ast::ConstantItem*>(sym.declaration);
    if (!decl || !id_decl_.count(decl)) {
        diag_.add_entry(diagnostic::Severity::Error, seg.span.begin, "constant is not registered");
        return std::nullopt;
    }
    return id_decl_.at(decl);
}

std::optional<SymbolId> ConstEvaluator::resolveStructPrefix(const ast::PathExprSegment& seg, ResolveContext ctx) {
    if (seg.ident_segment.is_self) {
        diag_.add_entry(diagnostic::Severity::Error, seg.span.begin, "'self' is value name");
        return std::nullopt;
    }
    if (seg.ident_segment.name.has_value()) {
        auto it = index_.type_names.find(*seg.ident_segment.name);
        if (it == index_.type_names.end()) {
            diag_.add_entry(diagnostic::Severity::Error, seg.span.begin, "unresolved struct name");
            return std::nullopt;
        }
        const auto& sym = index_.symbols.at(it->second);
        if (sym.kind != SymbolKind::Struct) {
            diag_.add_entry(diagnostic::Severity::Error, seg.span.begin, "path does not name a struct");
            return std::nullopt;
        }
        const auto* decl = dynamic_cast<const ast::StructItem*>(sym.declaration);
        if (!decl || !model_.structs_.count(sym.id)) {
            diag_.add_entry(diagnostic::Severity::Error, seg.span.begin, "struct is not registered");
            return std::nullopt;
        }
        return sym.id;
    } else if (seg.ident_segment.is_Self) {
        if (!ctx.self_type.has_value()) {
            diag_.add_entry(diagnostic::Severity::Error, seg.span.begin, "'Self' has no struct context");
            return std::nullopt;
        }
        return *ctx.self_type;
    }
    diag_.add_entry(diagnostic::Severity::Error, seg.span.begin, "empty ident segment");
    return std::nullopt;
}

std::optional<ConstId> ConstEvaluator::resolveAssociatedConstant(SymbolId id, const ast::PathExprSegment& seg) {
    if (!seg.ident_segment.name.has_value()) {
        diag_.add_entry(diagnostic::Severity::Error, seg.span.begin, "constant path has empty name");
        return std::nullopt;
    }
    const std::string& name = *seg.ident_segment.name;
    auto owner_it = model_.assoc_by_struct_.find(id);
    if (owner_it == model_.assoc_by_struct_.end()) {
        diag_.add_entry(diagnostic::Severity::Error, seg.span.begin, "struct has no associated constant");
        return std::nullopt;
    }
    auto member_it = owner_it->second.find(name);
    if (member_it == owner_it->second.end() || member_it->second >= model_.assocs_.size()) {
        diag_.add_entry(diagnostic::Severity::Error, seg.span.begin, "struct has no associated constant");
        return std::nullopt;
    }
    const auto& assoc = model_.assocs_[member_it->second];
    if (assoc.owner != id || !assoc.const_decl || assoc.kind != AssocKind::Constant) {
        diag_.add_entry(diagnostic::Severity::Error, seg.span.begin, "associated item is not a constant");
        return std::nullopt;
    }
    const auto* decl = assoc.const_decl;
    auto constant_it = id_decl_.find(decl);
    if (constant_it == id_decl_.end()) {
        diag_.add_entry(diagnostic::Severity::Error, seg.span.begin, "associated constant is not registered");
        return std::nullopt;
    }
    return constant_it->second;
}

bool ConstEvaluator::addPathDependency(ConstId source, const ast::PathInExpression& path, ResolveContext ctx) {
    if (source >= dependencies_.size()) {
        diag_.add_entry(diagnostic::Severity::Error, path.span.begin, "invalid constant source");
        return false;
    }
    const auto target = resolvePath(path, ctx);
    if (!target.has_value()) {
        return false;
    }
    if (*target >= definitions_.size()) {
        diag_.add_entry(diagnostic::Severity::Error, path.span.begin, "invalid path constant");
        return false;
    }
    dependencies_[source].push_back(*target);
    return true;
}

bool ConstEvaluator::collectReferences(ConstId source, const ast::ConstValue& value, ResolveContext ctx) {
    switch (value.type) {
        case ast::ConstValueType::Integer:
        case ast::ConstValueType::Boolean:
            return true;
        case ast::ConstValueType::ConstantPath:
            if (!value.path.has_value()) {
                diag_.add_entry(diagnostic::Severity::Error, value.span.begin, "unexpected empty constant path");
                return false;
            }
            return addPathDependency(source, *value.path, ctx);
        case ast::ConstValueType::NegatedMagnitude:
            if (!value.magnitude) {
                diag_.add_entry(diagnostic::Severity::Error, value.span.begin, "unexpected empty magnitude");
                return false;
            }
            return collectMagnitudeReferences(source, *value.magnitude, ctx);
        case ast::ConstValueType::Parenthesized:
            if (!value.inner) {
                diag_.add_entry(diagnostic::Severity::Error, value.span.begin, "unexpected empty inner constant");
                return false;
            }
            return collectReferences(source, *value.inner, ctx);
    }
    diag_.add_entry(diagnostic::Severity::Error, value.span.begin, "unexpected constant type");
    return false;
}

bool ConstEvaluator::collectMagnitudeReferences(ConstId source, const ast::Magnitude& mag, ResolveContext ctx) {
    switch (mag.type) {
        case ast::MagnitudeType::IntegerLiteral:
            return true;
        case ast::MagnitudeType::ConstantPath:
            if (!mag.path.has_value()) {
                diag_.add_entry(diagnostic::Severity::Error, mag.span.begin, "unexpected empty constant path");
                return false;
            }
            return addPathDependency(source, *mag.path, ctx);
        case ast::MagnitudeType::Parenthesized:
            if (!mag.inner) {
                diag_.add_entry(diagnostic::Severity::Error, mag.span.begin, "unexpected empty inner constant");
                return false;
            }
            return collectMagnitudeReferences(source, *mag.inner, ctx);
    }
    diag_.add_entry(diagnostic::Severity::Error, mag.span.begin, "unexpected magnitude type");
    return false;
}

bool ConstEvaluator::buildDependencies() {
    dependencies_.assign(definitions_.size(), {});
    bool ok = true;
    for (ConstId source = 0; source < definitions_.size(); source++) {
        const auto& def = definitions_[source];
        if (!def.declaration || !def.declaration->value) {
            const auto location = def.declaration ? def.declaration->span.begin : ast::SourceLocation {0, 0, 0};
            diag_.add_entry(diagnostic::Severity::Error, location, "unexpected empty constant definition");
            ok = false;
            continue;
        }
        ResolveContext ctx {def.owner};
        if (!collectReferences(source, *def.declaration->value, ctx)) {
            ok = false;
        }
    }
    return ok;
}

bool ConstEvaluator::checkCycles() {
    state_.assign(definitions_.size(), ConstState::Unknown);
    if (!buildDependencies()) {
        return false;
    }
    bool ok = true;
    for (size_t i = 0; i < definitions_.size(); i++) {
        if (state_[i] == ConstState::Unknown) {
            if(!visit(i)) {
                ok = false;
            }
        }
    }
    return ok;
}

bool ConstEvaluator::visit(ConstId id) {
    if (id >= definitions_.size()) {
        return false;
    }
    if (state_[id] == ConstState::Visited) {
        return true;
    }
    if (state_[id] == ConstState::Visiting) {
        const auto* decl = definitions_[id].declaration;
        const auto location = decl ? decl->span.begin : ast::SourceLocation {0, 0, 0};
        const auto name = decl ? decl->name : std::string("<unknown>");
        diag_.add_entry(diagnostic::Severity::Error, location, "constant dependency cycle involving " + name);
        return false;
    }
    state_[id] = ConstState::Visiting;
    bool ok = true;
    for (ConstId to: dependencies_[id]) {
        if (to >= definitions_.size() || !visit(to)) {
            ok = false;
        }
    }
    state_[id] = ConstState::Visited;
    return ok;
}

bool ConstEvaluator::evaluateAll() {
    cache_.assign(definitions_.size(), {});
    if (!checkCycles()) {
        return false;
    }
    state_.assign(definitions_.size(), ConstState::Unknown);
    bool ok = true;
    for (size_t i = 0; i < definitions_.size(); i++) {
        auto value = evaluate(i);
        if (!value.has_value()) {
            const auto* decl = definitions_[i].declaration;
            const auto location = decl ? decl->span.begin : ast::SourceLocation {0, 0, 0};
            const auto name = decl ? decl->name : std::string("<unknown>");
            diag_.add_entry(diagnostic::Severity::Error, location, "failed to evaluate constant " + name);
            ok = false;
        }
    }
    return ok;
}

std::optional<EvaluatedConst> ConstEvaluator::evaluate(ConstId id) {
    if (id >= definitions_.size() || id >= state_.size() || id >= cache_.size()) {
        return std::nullopt;
    }
    if (state_[id] == ConstState::Evaluated) {
        return cache_[id];
    }
    if (state_[id] == ConstState::Failed) {
        return std::nullopt;
    }
    if (state_[id] == ConstState::Visiting) {
        const auto* decl = definitions_[id].declaration;
        const auto location = decl ? decl->span.begin : ast::SourceLocation {0, 0, 0};
        diag_.add_entry(diagnostic::Severity::Error, location, "constant evaluation encountered a dependency cycle");
        state_[id] = ConstState::Failed;
        return std::nullopt;
    }
    const auto& def = definitions_[id];
    if (!def.declaration || !def.declaration->value) {
        state_[id] = ConstState::Failed;
        return std::nullopt;
    }
    if (!def.type_resolved) {
        diag_.add_entry(diagnostic::Severity::Error, def.declaration->span.begin, "constant type was not resolved");
        state_[id] = ConstState::Failed;
        return std::nullopt;
    }
    state_[id] = ConstState::Visiting;
    ResolveContext ctx {def.owner};
    auto result = evaluateValue(*def.declaration->value, def.type, ctx);
    if (!result || result->type != def.type) {
        state_[id] = ConstState::Failed;
        return std::nullopt;
    }
    cache_[id] = *result;
    state_[id] = ConstState::Evaluated;
    return result;
}

std::optional<EvaluatedConst> ConstEvaluator::evaluateIntegerLiteral(
    const ast::IntegerLiteralValue& literal,
    std::optional<TyId> expected_type
) {
    uint64_t magnitude = 0;
    std::string parse_error;
    if (!parseIntegerMagnitude(literal, magnitude, parse_error)) {
        diag_.add_entry(diagnostic::Severity::Error, literal.span.begin, parse_error);
        return std::nullopt;
    }

    std::optional<PrimaryTyKind> expected_integer_kind;
    if (expected_type.has_value()) {
        const auto& expected_info = model_.typeContext().get(*expected_type);
        if (const auto* primary = std::get_if<PrimaryTy>(&expected_info)) {
            if (isIntegerKind(primary->ty)) {
                expected_integer_kind = primary->ty;
            }
        }
    }

    const PrimaryTyKind kind = suffixKind(literal.suffix).value_or(
        expected_integer_kind.value_or(PrimaryTyKind::I32)
    );
    const uint64_t max_magnitude = isSignedKind(kind)
        ? static_cast<uint64_t>(std::numeric_limits<int32_t>::max())
        : static_cast<uint64_t>(std::numeric_limits<uint32_t>::max());
    if (magnitude > max_magnitude) {
        diag_.add_entry(diagnostic::Severity::Error, literal.span.begin, "integer literal is out of range for its selected type");
        return std::nullopt;
    }

    const TyId type = model_.typeContext().insert(PrimaryTy {kind});
    ConstantValue constant;
    switch (kind) {
        case PrimaryTyKind::I32:
            constant = static_cast<int32_t>(magnitude);
            break;
        case PrimaryTyKind::U32:
            constant = static_cast<uint32_t>(magnitude);
            break;
        case PrimaryTyKind::ISize:
            constant = static_cast<int64_t>(magnitude);
            break;
        case PrimaryTyKind::USize:
            constant = static_cast<uint64_t>(magnitude);
            break;
        case PrimaryTyKind::Bool:
            diag_.add_entry(diagnostic::Severity::Error, literal.span.begin, "integer literal cannot have type bool");
            return std::nullopt;
    }
    return checkExpectedType(EvaluatedConst {type, std::move(constant)}, expected_type, literal.span.begin);
}

std::optional<EvaluatedConst> ConstEvaluator::evaluateNegatedMagnitude(const ast::Magnitude& mag, ResolveContext ctx) {
    switch (mag.type) {
        case ast::MagnitudeType::IntegerLiteral: {
            if (!mag.value.has_value()) {
                diag_.add_entry(diagnostic::Severity::Error, mag.span.begin, "unexpected empty integer magnitude");
                return std::nullopt;
            }
            uint64_t magnitude = 0;
            std::string parse_error;
            if (!parseIntegerMagnitude(*mag.value, magnitude, parse_error)) {
                diag_.add_entry(diagnostic::Severity::Error, mag.value->span.begin, parse_error);
                return std::nullopt;
            }
            const PrimaryTyKind kind = suffixKind(mag.value->suffix).value_or(PrimaryTyKind::I32);
            if (!isSignedKind(kind)) {
                diag_.add_entry(diagnostic::Severity::Error, mag.value->span.begin, "negative magnitude requires a signed integer type");
                return std::nullopt;
            }
            const uint64_t min_magnitude = static_cast<uint64_t>(std::numeric_limits<int32_t>::max()) + 1;
            if (magnitude > min_magnitude) {
                diag_.add_entry(diagnostic::Severity::Error, mag.value->span.begin, "negative integer literal is out of range for its selected type");
                return std::nullopt;
            }
            const int64_t signed_value = magnitude == min_magnitude
                ? static_cast<int64_t>(std::numeric_limits<int32_t>::min())
                : -static_cast<int64_t>(magnitude);
            ConstantValue constant;
            if (kind == PrimaryTyKind::I32) {
                constant = static_cast<int32_t>(signed_value);
            } else {
                constant = signed_value;
            }
            const TyId type = model_.typeContext().insert(PrimaryTy {kind});
            return EvaluatedConst {type, std::move(constant)};
        }
        case ast::MagnitudeType::ConstantPath: {
            if (!mag.path.has_value()) {
                diag_.add_entry(diagnostic::Severity::Error, mag.span.begin, "unexpected empty constant path in negative magnitude");
                return std::nullopt;
            }
            const auto target = resolvePath(*mag.path, ctx);
            if (!target.has_value()) {
                return std::nullopt;
            }
            auto result = evaluate(*target);
            if (!result.has_value()) {
                return std::nullopt;
            }
            const auto& type_info = model_.typeContext().get(result->type);
            const auto* primary = std::get_if<PrimaryTy>(&type_info);
            if (primary == nullptr || !isSignedKind(primary->ty)) {
                diag_.add_entry(diagnostic::Severity::Error, mag.span.begin, "negative constant path requires a signed integer constant");
                return std::nullopt;
            }

            int64_t value = 0;
            if (primary->ty == PrimaryTyKind::I32) {
                const auto* stored = std::get_if<int32_t>(&result->value);
                if (stored == nullptr) {
                    diag_.add_entry(diagnostic::Severity::Error, mag.span.begin, "constant value does not match its i32 type");
                    return std::nullopt;
                }
                value = *stored;
            } else {
                const auto* stored = std::get_if<int64_t>(&result->value);
                if (stored == nullptr) {
                    diag_.add_entry(diagnostic::Severity::Error, mag.span.begin, "constant value does not match its isize type");
                    return std::nullopt;
                }
                value = *stored;
            }
            if (value < std::numeric_limits<int32_t>::min() || value > std::numeric_limits<int32_t>::max()) {
                diag_.add_entry(diagnostic::Severity::Error, mag.span.begin, "signed constant is out of range for its type");
                return std::nullopt;
            }
            if (value == std::numeric_limits<int32_t>::min()) {
                diag_.add_entry(diagnostic::Severity::Error, mag.span.begin, "negating the signed minimum is out of range");
                return std::nullopt;
            }
            const int64_t negated = -value;
            if (primary->ty == PrimaryTyKind::I32) {
                result->value = static_cast<int32_t>(negated);
            } else {
                result->value = negated;
            }
            return result;
        }
        case ast::MagnitudeType::Parenthesized:
            if (!mag.inner) {
                diag_.add_entry(diagnostic::Severity::Error, mag.span.begin, "unexpected empty parenthesized magnitude");
                return std::nullopt;
            }
            return evaluateNegatedMagnitude(*mag.inner, ctx);
    }
    diag_.add_entry(diagnostic::Severity::Error, mag.span.begin, "unexpected magnitude type");
    return std::nullopt;
}

std::optional<EvaluatedConst> ConstEvaluator::evaluateMagnitude(const ast::Magnitude& mag, ResolveContext ctx) {
    switch (mag.type) {
        case ast::MagnitudeType::IntegerLiteral:
            if (!mag.value.has_value()) {
                diag_.add_entry(diagnostic::Severity::Error, mag.span.begin, "unexpected empty integer magnitude");
                return std::nullopt;
            }
            return evaluateIntegerLiteral(*mag.value, std::nullopt);
        case ast::MagnitudeType::ConstantPath:
            if (!mag.path.has_value()) {
                diag_.add_entry(diagnostic::Severity::Error, mag.span.begin, "unexpected empty constant path in magnitude");
                return std::nullopt;
            } else {
                const auto target = resolvePath(*mag.path, ctx);
                if (!target.has_value()) {
                    return std::nullopt;
                }
                return evaluate(*target);
            }
        case ast::MagnitudeType::Parenthesized:
            if (!mag.inner) {
                diag_.add_entry(diagnostic::Severity::Error, mag.span.begin, "unexpected empty parenthesized magnitude");
                return std::nullopt;
            }
            return evaluateMagnitude(*mag.inner, ctx);
    }
    diag_.add_entry(diagnostic::Severity::Error, mag.span.begin, "unexpected magnitude type");
    return std::nullopt;
}

std::optional<EvaluatedConst> ConstEvaluator::checkExpectedType(
    EvaluatedConst value,
    std::optional<TyId> expected_type,
    ast::SourceLocation location
) {
    if (expected_type.has_value() && value.type != *expected_type) {
        diag_.add_entry(diagnostic::Severity::Error, location, "constant value type does not match its expected type");
        return std::nullopt;
    }
    return value;
}

std::optional<EvaluatedConst> ConstEvaluator::evaluateValue(const ast::ConstValue& value, std::optional<TyId> expected_type, ResolveContext ctx) {
    switch (value.type) {
        case ast::ConstValueType::Integer: {
            if (!value.integer.has_value()) {
                diag_.add_entry(diagnostic::Severity::Error, value.span.begin, "unexpected empty integer literal");
                return std::nullopt;
            }
            return evaluateIntegerLiteral(*value.integer, expected_type);
        }
        case ast::ConstValueType::Boolean: {
            if (!value.boolean.has_value()) {
                diag_.add_entry(diagnostic::Severity::Error, value.span.begin, "unexpected empty boolean literal");
                return std::nullopt;
            }
            const TyId type = model_.typeContext().insert(PrimaryTy {PrimaryTyKind::Bool});
            return checkExpectedType(EvaluatedConst {type, *value.boolean}, expected_type, value.span.begin);
        }
        case ast::ConstValueType::ConstantPath: {
            if (!value.path.has_value()) {
                diag_.add_entry(diagnostic::Severity::Error, value.span.begin, "unexpected empty constant path");
                return std::nullopt;
            }
            const auto target = resolvePath(*value.path, ctx);
            if (!target.has_value()) {
                return std::nullopt;
            }
            auto result = evaluate(*target);
            if (!result.has_value()) {
                return std::nullopt;
            }
            return checkExpectedType(std::move(*result), expected_type, value.span.begin);
        }
        case ast::ConstValueType::NegatedMagnitude: {
            if (!value.magnitude) {
                diag_.add_entry(diagnostic::Severity::Error, value.span.begin, "unexpected empty magnitude");
                return std::nullopt;
            }
            auto result = evaluateNegatedMagnitude(*value.magnitude, ctx);
            if (!result.has_value()) {
                return std::nullopt;
            }
            return checkExpectedType(std::move(*result), expected_type, value.span.begin);
        }
        case ast::ConstValueType::Parenthesized:
            if (!value.inner) {
                diag_.add_entry(diagnostic::Severity::Error, value.span.begin, "unexpected empty parenthesized constant");
                return std::nullopt;
            }
            return evaluateValue(*value.inner, expected_type, ctx);
    }
    diag_.add_entry(diagnostic::Severity::Error, value.span.begin, "unexpected constant value type");
    return std::nullopt;
}

template <typename T>
std::optional<T> ConstEvaluator::evaluate(const ast::ConstantItem* constant) {
    const auto it = id_decl_.find(constant);
    if (it == id_decl_.end()) {
        return std::nullopt;
    }
    const ConstId id = it->second;
    if (id >= state_.size() || id >= cache_.size() || state_[id] != ConstState::Evaluated) {
        return std::nullopt;
    }
    const auto* result = std::get_if<T>(&cache_[id].value);
    if (result == nullptr) {
        return std::nullopt;
    }
    return *result;
}

template <typename T>
std::optional<T> ConstEvaluator::evaluate(const ast::ConstValue* constant) {
    return evaluate<T>(constant, std::nullopt, ResolveContext {});
}

template <typename T>
std::optional<T> ConstEvaluator::evaluate(
    const ast::ConstValue* constant,
    std::optional<TyId> expected_type,
    ResolveContext ctx
) {
    if (constant == nullptr) {
        return std::nullopt;
    }
    auto evaluated = evaluateValue(*constant, expected_type, ctx);
    if (!evaluated.has_value()) {
        return std::nullopt;
    }
    const auto* result = std::get_if<T>(&evaluated->value);
    if (result == nullptr) {
        diag_.add_entry(diagnostic::Severity::Error, constant->span.begin, "constant value representation does not match requested type");
        return std::nullopt;
    }
    return *result;
}

const EvaluatedConst* ConstEvaluator::findEvaluated(ConstId id) const {
    if (id >= cache_.size()) {
        return nullptr;
    }
    return &cache_[id];
}

std::optional<ConstId> ConstEvaluator::findConstantId(const ast::ConstantItem* declaration) const {
    auto it = id_decl_.find(declaration);
    if (it == id_decl_.end()) {
        return std::nullopt;
    }
    return it->second;
}

template std::optional<bool> ConstEvaluator::evaluate<bool>(const ast::ConstantItem* constant);
template std::optional<int32_t> ConstEvaluator::evaluate<int32_t>(const ast::ConstantItem* constant);
template std::optional<uint32_t> ConstEvaluator::evaluate<uint32_t>(const ast::ConstantItem* constant);
template std::optional<int64_t> ConstEvaluator::evaluate<int64_t>(const ast::ConstantItem* constant);
template std::optional<uint64_t> ConstEvaluator::evaluate<uint64_t>(const ast::ConstantItem* constant);
template std::optional<bool> ConstEvaluator::evaluate<bool>(const ast::ConstValue* constant);
template std::optional<int32_t> ConstEvaluator::evaluate<int32_t>(const ast::ConstValue* constant);
template std::optional<uint32_t> ConstEvaluator::evaluate<uint32_t>(const ast::ConstValue* constant);
template std::optional<int64_t> ConstEvaluator::evaluate<int64_t>(const ast::ConstValue* constant);
template std::optional<uint64_t> ConstEvaluator::evaluate<uint64_t>(const ast::ConstValue* constant);
template std::optional<bool> ConstEvaluator::evaluate<bool>(const ast::ConstValue* constant, std::optional<TyId> expected_type, ResolveContext ctx);
template std::optional<int32_t> ConstEvaluator::evaluate<int32_t>(const ast::ConstValue* constant, std::optional<TyId> expected_type, ResolveContext ctx);
template std::optional<uint32_t> ConstEvaluator::evaluate<uint32_t>(const ast::ConstValue* constant, std::optional<TyId> expected_type, ResolveContext ctx);
template std::optional<int64_t> ConstEvaluator::evaluate<int64_t>(const ast::ConstValue* constant, std::optional<TyId> expected_type, ResolveContext ctx);
template std::optional<uint64_t> ConstEvaluator::evaluate<uint64_t>(const ast::ConstValue* constant, std::optional<TyId> expected_type, ResolveContext ctx);

} // namespace semantic
