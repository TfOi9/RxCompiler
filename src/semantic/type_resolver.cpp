#include "type_resolver.hpp"
#include "ast/ast.hpp"
#include "diagnostic/diagnostic.hpp"
#include "semantic/generic_arguments.hpp"
#include "semantic/semantic_ids.hpp"
#include "semantic/symbol.hpp"
#include "semantic/types.hpp"
#include "semantic/const_evaluator.hpp"
#include <cstdint>
#include <limits>

namespace semantic {

TyId TypeResolver::resolve(const ast::TypeRef& ty, ResolveContext ctx) {
    if (const auto* unit = dynamic_cast<const ast::UnitType*>(&ty)) {
        return types_.insert(UnitTy());
    } else if (const auto* paren = dynamic_cast<const ast::ParenthesizedType*>(&ty)) {
        return resolve(*paren->type, ctx);
    } else if (const auto* ref = dynamic_cast<const ast::ReferenceType*>(&ty)) {
        TyId inner = resolve(*ref->type, ctx);
        if (inner == types_.error()) return inner;
        return types_.insert(RefTy {
            inner,
            ref->is_mut
        });
    } else if (const auto* arr = dynamic_cast<const ast::ArrayType*>(&ty)) {
        TyId inner = resolve(*arr->type, ctx);
        if (inner == types_.error()) return inner;
        if (!arr->length) {
            diag_.add_entry(diagnostic::Severity::Error, arr->span.begin, "array type has no length");
            return types_.error();
        }
        const TyId usize_type = types_.insert(PrimaryTy {PrimaryTyKind::USize});
        const auto evaluated_len = eval_.evaluate<uint64_t>(arr->length.get(), usize_type, ctx);
        if (!evaluated_len.has_value()) {
            return types_.error();
        }
        if (*evaluated_len > std::numeric_limits<uint32_t>::max()) {
            diag_.add_entry(diagnostic::Severity::Error, arr->length->span.begin, "array length exceeds usize range");
            return types_.error();
        }
        const uint32_t len = static_cast<uint32_t>(*evaluated_len);
        return types_.insert(ArrayTy {
            inner,
            len
        });
    } else if (const auto* path = dynamic_cast<const ast::TypePath*>(&ty)) {
        return resolveTypePath(*path, ctx);
    }
    diag_.add_entry(diagnostic::Severity::Error, ty.span.begin, "typeref has no proper type");
    return types_.error();
}

TyId TypeResolver::resolveTypePath(const ast::TypePath& path, ResolveContext ctx) {
    if (path.path_segments.empty()) {
        diag_.add_entry(diagnostic::Severity::Error, path.span.begin, "path has no segments");
        return types_.error();
    }
    if (path.path_segments.size() != 1) {
        diag_.add_entry(diagnostic::Severity::Error, path.span.begin, "path does not name a supported type");
        return types_.error();
    }
    const auto& segment = path.path_segments[0];
    const auto& ident = segment.ident_segment;
    SymbolId id = 0;
    const Symbol* sym = nullptr;
    if (ident.is_Self) {
        if (!ctx.self_type.has_value()) {
            diag_.add_entry(diagnostic::Severity::Error, ident.span.begin, "'Self' is not available in this context");
            return types_.error();
        }
        id = *ctx.self_type;
        sym = id < index_.symbols.size() ? &index_.symbols[id] : nullptr;
        if (sym == nullptr || sym->kind != SymbolKind::Struct) {
            diag_.add_entry(diagnostic::Severity::Error, ident.span.begin, "'Self' does not refer to a struct");
            return types_.error();
        }
    } else if (ident.is_self) {
        diag_.add_entry(diagnostic::Severity::Error, ident.span.begin, "'self' is a value, not a type");
        return types_.error();
    } else if (ident.name.has_value()) {
        if (!index_.type_names.count(*ident.name)) {
            diag_.add_entry(diagnostic::Severity::Error, ident.span.begin, "struct " + *ident.name + " not declared");
            return types_.error();
        }
        id = index_.type_names.find(*ident.name)->second;
        sym = id < index_.symbols.size() ? &index_.symbols[id] : nullptr;
        if (sym == nullptr) {
            diag_.add_entry(diagnostic::Severity::Error, ident.span.begin, "invalid type symbol");
            return types_.error();
        }
    } else {
        diag_.add_entry(diagnostic::Severity::Error, ident.span.begin, "expected type name");
        return types_.error();
    }
    const ast::GenericArgs* args = segment.generic_args ? &*segment.generic_args : nullptr;
    if (sym->kind == SymbolKind::Struct) {
        return resolveStructType(*sym, id, args, segment.span);
    }
    if (sym->kind != SymbolKind::BuiltinType) {
        diag_.add_entry(diagnostic::Severity::Error, segment.span.begin, "name does not refer to a type");
        return types_.error();
    }
    if (sym->name == "Box" || sym->name == "Vec") {
        return resolveContainerType(sym->name, args, segment.span, ctx);
    }
    if (auto pr = resolvePrimitive(sym->name)) {
        const size_t argcnt = args ? args->args.size() : 0;
        if (argcnt) {
            diag_.add_entry(diagnostic::Severity::Error, segment.span.begin, "primitive types do not take generic arguments");
            return types_.error();
        }
        return types_.insert(PrimaryTy {*pr});
    }
    diag_.add_entry(diagnostic::Severity::Error, segment.span.begin, "builtin is not a concrete type");
    return types_.error();
}

TyId TypeResolver::resolveStructType(const Symbol& symbol, SymbolId symbol_id, const ast::GenericArgs* args, const ast::SourceSpan& span) {
    const auto* decl = dynamic_cast<const ast::StructItem*>(symbol.declaration);
    if (decl == nullptr) {
        diag_.add_entry(diagnostic::Severity::Error, span.begin, "struct symbol has no declaration");
        return types_.error();
    }
    if (!checkLifetimeOnlyArguments(args, symbol.name, diag_)) {
        return types_.error();
    }
    return types_.insert(StructTy {symbol_id});
}

TyId TypeResolver::resolveContainerType(const std::string& name, const ast::GenericArgs* args, const ast::SourceSpan& span, ResolveContext ctx) {
    const ast::TypeRef* element_ast = requireSingleTypeArgument(args, span, name, diag_);
    if (!element_ast) {
        return types_.error();
    }
    TyId elem = resolve(*element_ast, ctx);
    if (elem == types_.error()) return elem;
    if (name == "Box") {
        return types_.insert(BoxTy {elem});
    } else {
        return types_.insert(VecTy {elem});
    }
}

std::optional<PrimaryTyKind> TypeResolver::resolvePrimitive(const std::string& name) {
    if (name == "bool") {
        return PrimaryTyKind::Bool;
    } else if (name == "i32") {
        return PrimaryTyKind::I32;
    } else if (name == "u32") {
        return PrimaryTyKind::U32;
    } else if (name == "isize") {
        return PrimaryTyKind::ISize;
    } else if (name == "usize") {
        return PrimaryTyKind::USize;
    }
    return std::nullopt;
}

TyId TypeResolver::resolveNamedType(const ast::PathIdentSegment& ident, const ast::GenericArgs* args, ast::SourceSpan span, ResolveContext ctx) {
    auto fail = [&](const std::string& message) -> TyId {
        diag_.add_entry(diagnostic::Severity::Error, span.begin, message);
        return types_.error();
    };
    SymbolId id = 0;
    if (ident.is_Self) {
        if (!ctx.self_type) {
            return fail("'Self' is unavailable in this context");
        }
        id = *ctx.self_type;
        if (id >= index_.symbols.size() || index_.symbols[id].kind != SymbolKind::Struct) {
            return fail("'Self' does not refer to a struct");
        }
    } else if (ident.is_self) {
        return fail("'self' is a value, not a type");
    } else if (ident.name) {
        auto found = index_.type_names.find(*ident.name);
        if (found == index_.type_names.end()) {
            return fail("unresolved type name " + *ident.name);
        }
        id = found->second;
        if (id >= index_.symbols.size()) {
            return fail("invalid type symbol");
        }
    } else {
        return fail("expected a type name");
    }
    const Symbol& symbol = index_.symbols[id];
    if (symbol.kind == SymbolKind::Struct) {
        return resolveStructType(symbol, id, args, span);
    }
    if (symbol.kind != SymbolKind::BuiltinType) {
        return fail("name does not refer to a type");
    }
    if (symbol.name == "Box" || symbol.name == "Vec") {
        return resolveContainerType(symbol.name, args, span, ctx);
    }
    if (auto primitive = resolvePrimitive(symbol.name)) {
        if (!checkNoGenericArguments(args, symbol.name, diag_)) {
            return fail("primitive types take no generic arguments");
        }
        return types_.insert(PrimaryTy{*primitive});
    }
    return fail("builtin name is not a concrete type");
}

} // namespace semantic
