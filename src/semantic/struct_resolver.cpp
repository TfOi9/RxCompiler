#include "struct_resolver.hpp"
#include "semantic_model.hpp"

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

} // namespace semantic