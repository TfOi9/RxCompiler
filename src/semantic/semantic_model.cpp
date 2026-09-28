#include "semantic_model.hpp"
#include "ast/ast.hpp"
#include "diagnostic/diagnostic.hpp"
#include "semantic/symbol.hpp"
#include "semantic/type_resolver.hpp"
#include "semantic/types.hpp"
#include <optional>
#include <variant>

namespace semantic {

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
            info.derives = *derives;
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

void DeriveChecker::checkDerive(SymbolId sub, DeriveSet derives) {
    const auto& sub_info = model_.structs_.at(sub);
    const auto& sub_derives = sub_info.derives;
    if (derives.has_copy && !sub_derives.has_copy) {
        check_passed_ = false;
        return;
    }
    if (derives.has_clone && !sub_derives.has_clone) {
        check_passed_ = false;
        return;
    }
    if (derives.has_partial_eq && !sub_derives.has_partial_eq) {
        check_passed_ = false;
        return;
    }
    if (derives.has_eq && !sub_derives.has_eq) {
        check_passed_ = false;
        return;
    }
    for (const auto& subs: sub_info.fields) {
        const auto& type_id = subs.type;
        const auto& type = model_.typeContext().get(type_id);
        if (const auto* stru = std::get_if<StructTy>(&type)) {
            checkDerive(stru->def, derives);
        }
    }
}

bool DeriveChecker::checkAll() {
    check_passed_ = true;
    for (const auto[id, stru]: model_.structs_) {
        checkDerive(id, stru.derives);
    }
    return check_passed_;
}

const FieldInfo* SemanticModel::findField(SymbolId owner, const std::string name) const {
    if (!structs_.count(owner)) {
        return nullptr;
    }
    const auto& stru = structs_.at(owner);
    if (!stru.field_name.count(name)) {
        return nullptr;
    }
    size_t field_id = stru.field_name.at(name);
    if (field_id >= stru.fields.size()) {
        return nullptr;
    }
    return &stru.fields[field_id];
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
    auto& members = model.assoc_by_struct[info.owner];
    if (members.count(info.name)) {
        const auto* decl = info.const_decl ? static_cast<const ast::Item*>(info.const_decl) : static_cast<const ast::Item*>(info.func_decl);
        diag.add_entry(diagnostic::Severity::Error, decl->span.begin, "duplicated associated item " + info.name);
        return false;
    }
    AssocId id = model.assocs_.size();
    info.id = id;
    model.assocs_.push_back(info);
    model.assoc_by_struct[info.owner][info.name] = id;
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
