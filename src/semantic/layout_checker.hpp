#pragma once
#include "symbol.hpp"
#include "types.hpp"

namespace semantic {

class SemanticModel;

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

} // namespace semantic