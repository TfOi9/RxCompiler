#pragma once
#include "../ast/ast.hpp"
#include "../diagnostic/diagnostic.hpp"
#include "semantic/semantic_model.hpp"
#include "type_resolver.hpp"
#include "symbol.hpp"
#include "symbol.hpp"
#include "semantic_ids.hpp"
#include "const_value.hpp"
#include <optional>
#include <unordered_map>
#include <vector>

namespace semantic {

class SemanticModel;
class TypeResolver;
class ResolveContext;

struct ConstantInfo {
    std::string name;
    const ast::ConstantItem* declaration;
    TyId type;
    ConstantValue value;
};

struct ConstDef {
    const ast::ConstantItem* declaration;
    std::optional<SymbolId> owner;
    TyId type;
    bool type_resolved = false;
};

enum class ConstState {
    Unknown,
    Visiting,
    Visited,
    Evaluated,
    Failed
};

class ConstEvaluator {
public:
    ConstEvaluator(const CrateIndex& index, SemanticModel& model, diagnostic::DiagnosticCollector& diag):
        index_(index), model_(model), diag_(diag) {}
    void collectDefinitions();
    void resolveTypes(TypeResolver& type_resolver);
    std::optional<ConstId> resolvePath(const ast::PathInExpression& path, ResolveContext ctx);
    bool checkCycles();
    bool evaluateAll();
    template <typename T>
    std::optional<T> evaluate(const ast::ConstantItem* constant);
    template <typename T>
    std::optional<T> evaluate(const ast::ConstValue* constant);
    template <typename T>
    std::optional<T> evaluate(const ast::ConstValue* constant, std::optional<TyId> expected_type, ResolveContext ctx);
    const EvaluatedConst* findEvaluated(ConstId id) const;
    std::optional<ConstId> findConstantId(const ast::ConstantItem* declaration) const;

private:
    const CrateIndex& index_;
    SemanticModel& model_;
    diagnostic::DiagnosticCollector& diag_;
    std::vector<ConstDef> definitions_;
    std::unordered_map<const ast::ConstantItem*, ConstId> id_decl_;
    std::vector<std::vector<ConstId>> dependencies_;
    std::vector<ConstState> state_;
    std::vector<EvaluatedConst> cache_;

    std::optional<ConstId> resolveTopLevelConstant(const ast::PathExprSegment& seg);
    std::optional<SymbolId> resolveStructPrefix(const ast::PathExprSegment& seg, ResolveContext ctx);
    std::optional<ConstId> resolveAssociatedConstant(SymbolId id, const ast::PathExprSegment& seg);

    bool buildDependencies();
    bool collectReferences(ConstId source, const ast::ConstValue& value, ResolveContext ctx);
    bool collectMagnitudeReferences(ConstId source, const ast::Magnitude& mag, ResolveContext ctx);
    bool addPathDependency(ConstId source, const ast::PathInExpression& path, ResolveContext ctx);

    bool visit(ConstId id);

    std::optional<EvaluatedConst> evaluate(ConstId id);
    std::optional<EvaluatedConst> evaluateValue(const ast::ConstValue& value, std::optional<TyId> expected_type, ResolveContext ctx);
    std::optional<EvaluatedConst> evaluateIntegerLiteral(const ast::IntegerLiteralValue& literal, std::optional<TyId> expected_type);
    std::optional<EvaluatedConst> evaluateNegatedMagnitude(const ast::Magnitude& mag, ResolveContext ctx);
    std::optional<EvaluatedConst> checkExpectedType(EvaluatedConst value, std::optional<TyId> expected_type, ast::SourceLocation location);
    std::optional<EvaluatedConst> evaluateMagnitude(const ast::Magnitude& mag, ResolveContext ctx);
};

} // namespace semantic
