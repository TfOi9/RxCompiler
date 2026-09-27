#include "semantic_model.hpp"
#include "ast/ast.hpp"
#include "diagnostic/diagnostic.hpp"
#include "semantic/symbol.hpp"
#include "semantic/type_resolver.hpp"

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
        for (const auto& field: decl->struct_fields) {
            const auto& type = field.type;
            if (type == nullptr) {
                diag.add_entry(diagnostic::Severity::Error, type->span.begin, "unexpected empty type");
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

StructInfo* SemanticModel::findStruct(SymbolId id) {
    return structs_.count(id) ? &structs_[id] : nullptr;
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

} // namespace semantic