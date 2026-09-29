#include "layout_checker.hpp"
#include "semantic_model.hpp"
#include "struct_resolver.hpp"

namespace semantic {

void LayoutChecker::collectDependencies() {
    for (const auto[id, stru]: model_.structs_) {
        for (const auto& field: stru.fields) {
            collectInlineTargets(id, field.type, field.declaration);
        }
    }
}

void LayoutChecker::collectInlineTargets(SymbolId owner, TyId type, const ast::StructField* field) {
    const TyInfo& info = model_.typeContext().get(type);
    if (const auto* stru = std::get_if<StructTy>(&info)) {
        edges_[owner].push_back(LayoutEdge {
            owner,
            stru->def,
            field
        });
        return;
    }
    if (const auto* arr = std::get_if<ArrayTy>(&info)) {
        collectInlineTargets(owner, arr->elem, field);
        return;
    }
}

void LayoutChecker::visit(SymbolId id) {
    state_[id] = VisitState::Visiting;
    for (const auto& edge: edges_[id]) {
        if (state_[edge.to] == VisitState::Visiting) {
            diag_.add_entry(diagnostic::Severity::Error, edge.field->span.begin, "recursive type has infinite size");
            check_passed_ = false;
        } else if (state_[edge.to] == VisitState::Unknown) {
            visit(edge.to);
        }
    }
    state_[id] = VisitState::Visited;
}

bool LayoutChecker::checkAll() {
    check_passed_ = true;
    state_.clear();
    edges_.clear();
    collectDependencies();
    for (const auto[id, stru]: model_.structs_) {
        if (state_[id] == VisitState::Unknown) visit(id);
    }
    return check_passed_;
}

} // namespace semantic