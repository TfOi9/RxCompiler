#pragma once
#include "../ast/ast.hpp"
#include "../diagnostic/diagnostic.hpp"
#include "semantic/symbol.hpp"
#include "type_resolver.hpp"
#include "const_evaluator.hpp"
#include "types.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace semantic {

struct FieldInfo {
    std::string name;
    TyId type;
    size_t ordinal;
    const ast::StructField* declaration;
};

enum class DeriveKind {
    Copy, Clone, PartialEq, Eq
};

struct DeriveSet {
    bool has_copy = false;
    bool has_clone = false;
    bool has_partial_eq = false;
    bool has_eq = false;
};

struct StructInfo {
    std::string name;
    const ast::StructItem* declaration;
    std::vector<FieldInfo> fields;
    std::unordered_map<std::string, size_t> field_name;
    DeriveSet derives;
};

class SemanticModel {
    friend class StructResolver;
public:
    StructInfo* findStruct(SymbolId id);
    const FieldInfo* findField(SymbolId owner, const std::string name) const;

private:
    std::unordered_map<SymbolId, StructInfo> structs_;
};

class StructResolver {
public:
    void declareAll(const CrateIndex& index, SemanticModel& model);
    void resolveAll(SemanticModel& model, TypeResolver& type_resolver, ConstEvaluator& const_evaluator, diagnostic::DiagnosticCollector& diag);
};

} // namespace semantic