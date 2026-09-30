#pragma once
#include "semantic/type_resolver.hpp"
#include "symbol.hpp"
#include <cstdint>

namespace semantic {

class SemanticModel;
class TypeResolver;

using ImplId = uint32_t;
using AssocId = uint32_t;

enum class AssocKind {
    Function,
    Constant
};

struct ImplInfo {
    ImplId id;
    const ast::ImplItem* declaration;
    SymbolId target_struct;
};

struct AssocInfo {
    AssocId id;
    SymbolId owner;
    AssocKind kind;
    std::string name;
    const ast::FunctionItem* func_decl;
    const ast::ConstantItem* const_decl;
};

class ImplResolver {
public:
    bool collectHeaders(const CrateIndex& index, TypeResolver& type_resolver, SemanticModel& model, diagnostic::DiagnosticCollector& diag);
    
private:
    std::optional<SymbolId> resolveTarget(const ast::ImplItem& impl, TypeResolver& type_resolver, SemanticModel& model, diagnostic::DiagnosticCollector& diag);
    bool registerAssoc(AssocInfo info, SemanticModel& model, diagnostic::DiagnosticCollector& diag);
};


} // namespace semantic