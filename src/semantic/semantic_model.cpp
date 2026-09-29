#include "semantic_model.hpp"
#include "ast/ast.hpp"
#include "diagnostic/diagnostic.hpp"
#include "semantic/symbol.hpp"
#include "semantic/type_resolver.hpp"
#include "semantic/types.hpp"
#include <optional>
#include <variant>

namespace semantic {

bool DeriveSet::has(DeriveKind kind) const {
    switch (kind) {
        case DeriveKind::Copy:
            return has_copy;
        case DeriveKind::Clone:
            return has_clone;
        case DeriveKind::PartialEq:
            return has_partial_eq;
        case DeriveKind::Eq:
            return has_eq;
    }
}

void DeriveSet::set(DeriveKind kind, bool value) {
    switch (kind) {
        case DeriveKind::Copy:
            has_copy = value; break;
        case DeriveKind::Clone:
            has_clone = value; break;
        case DeriveKind::PartialEq:
            has_partial_eq = value; break;
        case DeriveKind::Eq:
            has_eq = value; break;
    }
}

void StructResolver::declareAll(const CrateIndex& index, SemanticModel& model) {
    for (auto it: index.item_symbols) {
        const auto* stru = dynamic_cast<const ast::StructItem*>(it.first);
        if (!stru) {
            continue;
        }
        SymbolId id = it.second;
        if (!model.structs_.count(id)) {
            model.structs_[id] = StructInfo {
                stru->name,
                stru
            };
        }
    }
}

void StructResolver::resolveAll(SemanticModel& model, TypeResolver& type_resolver, ConstEvaluator& const_evaluator, diagnostic::DiagnosticCollector& diag) {
    for (auto it: model.structs_) {
        SymbolId id = it.first;
        StructInfo& info = it.second;
        const auto* decl = info.declaration;
        auto derives = getDerives(decl);
        if (!derives.has_value()) {
            diag.add_entry(diagnostic::Severity::Error, decl->span.begin, "duplicated attribute entries");
        } else {
            info.derives.requested = *derives;
        }
        for (const auto& field: decl->struct_fields) {
            const auto& type = field.type;
            if (type == nullptr) {
                diag.add_entry(diagnostic::Severity::Error, field.span.begin, "unexpected empty type");
                continue;
            }
            TyId type_id = type_resolver.resolve(*type, ResolveContext {id});
            if (info.field_name.count(field.name)) {
                diag.add_entry(diagnostic::Severity::Error, type->span.begin, "redefinition of struct field " + field.name);
                continue;
            }
            info.field_name[field.name] = info.fields.size();
            info.fields.push_back(FieldInfo {
                field.name,
                type_id,
                info.fields.size(),
                &field
            });
        }
    }
}

std::optional<DeriveSet> StructResolver::getDerives(const ast::StructItem* item) {
    DeriveSet set;
    const auto& attrs = item->attributes;
    for (const auto& attr: attrs) {
        const auto& derives = attr.derive_names;
        for (auto d: derives) {
            switch (d) {
                case ast::DeriveName::Copy:
                    if (set.has_copy) return std::nullopt;
                    set.has_copy = true;
                    break;
                case ast::DeriveName::Clone:
                    if (set.has_clone) return std::nullopt;
                    set.has_clone = true;
                    break;
                case ast::DeriveName::PartialEq:
                    if (set.has_partial_eq) return std::nullopt;
                    set.has_partial_eq = true;
                    break;
                case ast::DeriveName::Eq:
                    if (set.has_eq) return std::nullopt;
                    set.has_eq = true;
                    break;
            }
        }
    }
    return set;
}

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

StructInfo* SemanticModel::findStruct(SymbolId id) {
    return structs_.count(id) ? &structs_[id] : nullptr;
}

std::string DeriveChecker::kindName(DeriveKind trait) {
    switch (trait) {
        case DeriveKind::Copy: return "Copy";
        case DeriveKind::Clone: return "Clone";
        case DeriveKind::PartialEq: return "PartialEq";
        case DeriveKind::Eq: return "Eq";
    }
}

bool DeriveChecker::supportWith(TyId type, DeriveKind trait) const {
    const auto& info = model_.types_.get(type);
    if (std::holds_alternative<ErrorTy>(info)) {
        return true;
    }
    if (std::holds_alternative<UnitTy>(info)) {
        return true;
    }
    if (std::holds_alternative<PrimaryTy>(info)) {
        return true;
    }
    if (std::holds_alternative<NeverTy>(info)) {
        return true;
    }
    if (const auto* ref = std::get_if<RefTy>(&info)) {
        if (trait == DeriveKind::Copy || trait == DeriveKind::Clone) {
            return !ref->is_mut;
        } else {
            return supportWith(ref->target, trait);
        }
    }
    if (const auto* arr = std::get_if<ArrayTy>(&info)) {
        return supportWith(arr->elem, trait);
    }
    if (const auto* box = std::get_if<BoxTy>(&info)) {
        if (trait == DeriveKind::Copy) {
            return false;
        } else {
            return supportWith(box->elem, trait);
        }
    }
    if (const auto* vec = std::get_if<VecTy>(&info)) {
        if (trait == DeriveKind::Copy) {
            return false;
        } else {
            return supportWith(vec->elem, trait);
        }
    }
    if (const auto* stru = std::get_if<StructTy>(&info)) {
        auto it = model_.structs_.find(stru->def);
        return it != model_.structs_.end() && it->second.derives.valid.has(trait);
    }
    return false;
}

bool DeriveChecker::structRequirementsHold(const StructInfo& info, DeriveKind trait) const {
    if (trait == DeriveKind::Copy && !info.derives.requested.has(DeriveKind::Clone)) {
        return false;
    }
    if (trait == DeriveKind::Eq && !info.derives.requested.has(DeriveKind::PartialEq)) {
        return false;
    }
    for (const auto& field: info.fields) {
        if (!supportWith(field.type, trait)) {
            return false;
        }
    }
    return true;
}

void DeriveChecker::computeValid() {
    for (auto& [id, info]: model_.structs_) {
        info.derives.valid = info.derives.requested;
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& [id, info]: model_.structs_) {
            for (DeriveKind k: {DeriveKind::Copy, DeriveKind::Clone, DeriveKind::PartialEq, DeriveKind::Eq}) {
                if (info.derives.valid.has(k) && !structRequirementsHold(info, k)) {
                    info.derives.valid.set(k, false);
                    changed = true;
                }
            }
        }
    }
    computed_ = true;
}

bool DeriveChecker::reportStructErrors() {
    bool ok = true;
    for (auto& [id, info]: model_.structs_) {
        for (DeriveKind k: {DeriveKind::Copy, DeriveKind::Clone, DeriveKind::PartialEq, DeriveKind::Eq}) {
            if (info.derives.valid.has(k) || !info.derives.requested.has(k)) continue;
            if (k == DeriveKind::Copy && !info.derives.requested.has(DeriveKind::Clone)) {
                diag_.add_entry(diagnostic::Severity::Error, info.declaration->span.begin, "missing Clone trait for struct " + info.name);
            } else if (k == DeriveKind::Eq && !info.derives.requested.has(DeriveKind::PartialEq)) {
                diag_.add_entry(diagnostic::Severity::Error, info.declaration->span.begin, "missing PartialEq trait for struct " + info.name);
            } else {
                for (const auto& field: info.fields) {
                    if (!supportWith(field.type, k)) {
                        diag_.add_entry(diagnostic::Severity::Error, field.declaration->span.begin, "field " + field.name + " of struct " + info.name + " is missing trait " + kindName(k));
                        break;
                    }
                }
            }
            ok = false;
        }
    }
    reported_ = true;
    return ok;
}

bool DeriveChecker::checkAll() {
    computeValid();
    return reportStructErrors();
}

bool DeriveChecker::supports(TyId type, DeriveKind trait) {
    if (!computed_) {
        computeValid();
    }
    if (!reported_) {
        reportStructErrors();
    }
    return supportWith(type, trait);
}

std::optional<SymbolId> ImplResolver::resolveTarget(const ast::ImplItem& impl, TypeResolver& type_resolver, SemanticModel& model, diagnostic::DiagnosticCollector& diag) {
    if (!impl.type) {
        diag.add_entry(diagnostic::Severity::Error, impl.span.begin, "implementation has no type");
        return std::nullopt;
    }
    TyId target_type = type_resolver.resolve(*impl.type, ResolveContext {});
    const TyInfo& type_info = model.typeContext().get(target_type);
    if (std::holds_alternative<ErrorTy>(type_info)) {
        return std::nullopt;
    }
    const auto* stru = std::get_if<StructTy>(&type_info);
    if (!stru) {
        diag.add_entry(diagnostic::Severity::Error, impl.span.begin, "implementation target must be a user struct");
        return std::nullopt;
    }
    return stru->def;
}

bool ImplResolver::registerAssoc(AssocInfo info, SemanticModel& model, diagnostic::DiagnosticCollector& diag) {
    auto& members = model.assoc_by_struct_[info.owner];
    if (members.count(info.name)) {
        const auto* decl = info.const_decl ? static_cast<const ast::Item*>(info.const_decl) : static_cast<const ast::Item*>(info.func_decl);
        diag.add_entry(diagnostic::Severity::Error, decl->span.begin, "duplicated associated item " + info.name);
        return false;
    }
    AssocId id = model.assocs_.size();
    info.id = id;
    model.assocs_.push_back(info);
    model.assoc_by_struct_[info.owner][info.name] = id;
    if (info.const_decl) model.assoc_ids_[info.const_decl] = id;
    else model.assoc_ids_[info.func_decl] = id;
    return true;
}

bool ImplResolver::collectHeaders(const CrateIndex& index, TypeResolver& type_resolver, SemanticModel& model, diagnostic::DiagnosticCollector& diag) {
    bool success = true;
    for (const auto impl: index.pending_impls) {
        ImplId id = model.impls_.size();
        auto sym = resolveTarget(*impl, type_resolver, model, diag);
        if (!sym.has_value()) {
            success = false;
            continue;
        }
        ImplInfo info = {
            id,
            impl,
            *sym
        };
        model.impls_.push_back(info);
        model.impl_by_struct_[*sym].push_back(id);
        model.impl_ids_[impl] = id;
        for (const auto& assoc: impl->associated_items) {
            if (assoc.constant && *assoc.constant) {
                const auto* constant = assoc.constant->get();
                AssocInfo info = {
                    0,
                    *sym,
                    AssocKind::Constant,
                    constant->name,
                    nullptr,
                    constant
                };
                if (!registerAssoc(info, model, diag)) success = false;
            } else if (assoc.function && *assoc.function) {
                const auto* function = assoc.function->get();
                AssocInfo info = {
                    0,
                    *sym,
                    AssocKind::Function,
                    function->name,
                    function,
                    nullptr
                };
                if (!registerAssoc(info, model, diag)) success = false;
            }
        }
    }
    return success;
}

} // namespace semantic
