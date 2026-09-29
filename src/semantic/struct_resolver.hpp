#pragma once
#include "types.hpp"
#include <string>
#include <vector>

namespace semantic {

class SemanticModel;
class TypeResolver;
class ConstEvaluator;

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

    bool has(DeriveKind kind) const;
    void set(DeriveKind kind, bool value);
};

struct DeriveInfo {
    DeriveSet requested;
    DeriveSet valid;
};

struct StructInfo {
    std::string name;
    const ast::StructItem* declaration;
    std::vector<FieldInfo> fields;
    std::unordered_map<std::string, size_t> field_name;
    DeriveInfo derives;
};

class StructResolver {
public:
    void declareAll(const CrateIndex& index, SemanticModel& model);
    void resolveAll(SemanticModel& model, TypeResolver& type_resolver, ConstEvaluator& const_evaluator, diagnostic::DiagnosticCollector& diag);

private:
    std::optional<DeriveSet> getDerives(const ast::StructItem* item);
};

} // namespace semantic