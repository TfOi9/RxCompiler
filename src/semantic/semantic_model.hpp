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
#include <variant>

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

using ConstantValue = std::variant<bool, int32_t, uint32_t, int64_t, uint64_t>;

struct ConstantInfo {
    std::string name;
    const ast::ConstantItem* declaration;
    TyId type;
    ConstantValue value;
};

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

class SemanticModel {
    friend class StructResolver;
    friend class LayoutChecker;
    friend class DeriveChecker;
    friend class ImplResolver;
public:
    TypeContext& typeContext() { return types_; }
    const TypeContext& typeContext() const { return types_; }

    StructInfo* findStruct(SymbolId id);
    const FieldInfo* findField(SymbolId owner, const std::string name) const;

private:
    TypeContext types_;
    std::unordered_map<SymbolId, StructInfo> structs_;
    std::vector<ImplInfo> impls_;
    std::vector<AssocInfo> assocs_;
    std::unordered_map<SymbolId, std::vector<ImplId>> impl_by_struct_;
    std::unordered_map<SymbolId, std::unordered_map<std::string, AssocId>> assoc_by_struct;
    std::unordered_map<const ast::ImplItem*, ImplId> impl_ids_;
    std::unordered_map<const ast::Item*, AssocId> assoc_ids_;
};

class StructResolver {
public:
    void declareAll(const CrateIndex& index, SemanticModel& model);
    void resolveAll(SemanticModel& model, TypeResolver& type_resolver, ConstEvaluator& const_evaluator, diagnostic::DiagnosticCollector& diag);

private:
    std::optional<DeriveSet> getDerives(const ast::StructItem* item);
};

class LayoutChecker {
public:
    LayoutChecker(const SemanticModel& model, const CrateIndex& index, diagnostic::DiagnosticCollector& diag):
        model_(model), index_(index), diag_(diag) {}
    bool checkAll();

private:
    struct LayoutEdge {
        SymbolId from;
        SymbolId to;
        const ast::StructField* field;
    };
    enum class VisitState {
        Unknown,
        Visiting,
        Visited
    };

    void collectDependencies();
    void collectInlineTargets(SymbolId owner, TyId type, const ast::StructField* field);
    void visit(SymbolId id);

    const SemanticModel& model_;
    const CrateIndex& index_;
    diagnostic::DiagnosticCollector& diag_;
    bool check_passed_;

    std::unordered_map<SymbolId, VisitState> state_;
    std::unordered_map<SymbolId, std::vector<LayoutEdge>> edges_;
};

class DeriveChecker {
public:
    DeriveChecker(const SemanticModel& model, const CrateIndex& index, diagnostic::DiagnosticCollector& diag):
        model_(model), index_(index), diag_(diag) {}
    bool checkAll();

private:
    void checkDerive(SymbolId sub, DeriveSet derives);

    const SemanticModel& model_;
    const CrateIndex& index_;
    diagnostic::DiagnosticCollector& diag_;
    bool check_passed_;
};

class ImplResolver {
public:
    bool collectHeaders(const CrateIndex& index, TypeResolver& type_resolver, SemanticModel& model, diagnostic::DiagnosticCollector& diag);
    bool resolveSignatures(SemanticModel& model, TypeResolver& type_resolver, diagnostic::DiagnosticCollector& diag);

private:
    std::optional<SymbolId> resolveTarget(const ast::ImplItem& impl, TypeResolver& type_resolver, SemanticModel& model, diagnostic::DiagnosticCollector& diag);
    bool registerAssoc(AssocInfo info, SemanticModel& model, diagnostic::DiagnosticCollector& diag);
};

} // namespace semantic
