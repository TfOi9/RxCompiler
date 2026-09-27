#pragma once
#include "../ast/ast.hpp"
#include "semantic/symbol.hpp"
#include "types.hpp"
#include "../diagnostic/diagnostic.hpp"
#include <optional>

namespace semantic {

struct ResolveContext {
    std::optional<SymbolId> self_type;
};

class TypeResolver {
public:
    TypeResolver(TypeContext& types, const CrateIndex& index, diagnostic::DiagnosticCollector& diag):types_(types), index_(index), diag_(diag) {}
    TyId resolve(const ast::TypeRef& ty, ResolveContext ctx);

private:
    TypeContext& types_;
    const CrateIndex& index_;
    diagnostic::DiagnosticCollector& diag_;
    // ConstEvaluator eval_;

    TyId resolveTypePath(const ast::TypePath& path, ResolveContext ctx);
    TyId resolveStructType(const Symbol& symbol, SymbolId symbol_id, const ast::GenericArgs* args, const ast::SourceSpan& span);
    TyId resolveContainerType(const std::string& name, const ast::GenericArgs* args, const ast::SourceSpan& span, ResolveContext ctx);
    std::optional<PrimaryTyKind> resolvePrimitive(const std::string& name);
};

} // namespace semantic
