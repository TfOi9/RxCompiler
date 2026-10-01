#include "semantic_model.hpp"
#include "ast/ast.hpp"
#include "semantic/body_semantics.hpp"
#include "semantic/function_resolver.hpp"
#include "semantic/semantic_ids.hpp"
#include "struct_resolver.hpp"
#include <optional>

namespace semantic {

const FunctionInfo* SemanticModel::findFunction(FunctionId id) const {
    if (id >= functions_.size()) {
        return nullptr;
    }
    return &functions_[id];
}

std::optional<FunctionId> SemanticModel::findFunctionId(const ast::FunctionItem* declaration) const {
    if (!function_ids_.count(declaration)) {
        return std::nullopt;
    }
    return function_ids_.at(declaration);
}

std::optional<FunctionId> SemanticModel::findTopLevelFunction(SymbolId symbol) const {
    if (!top_level_function_ids_.count(symbol)) {
        return std::nullopt;
    }
    return top_level_function_ids_.at(symbol);
}

std::optional<FunctionId> SemanticModel::findAssociatedFunction(AssocId associated) const {
    if (!associated_function_ids_.count(associated)) {
        return std::nullopt;
    }
    return associated_function_ids_.at(associated);
}

const AssocInfo* SemanticModel::findAssociated(SymbolId owner, const std::string& name) const {
    if (!assoc_by_struct_.count(owner)) {
        return nullptr;
    }
    if (!assoc_by_struct_.at(owner).count(name)) {
        return nullptr;
    }
    auto assoc_id = assoc_by_struct_.at(owner).at(name);
    if (assoc_id >= assocs_.size()) {
        return nullptr;
    }
    return &assocs_[assoc_id];
}

const StructInfo* SemanticModel::findStruct(SymbolId id) const {
    if (!structs_.count(id)) {
        return nullptr;
    }
    return &structs_.at(id);
}

const FieldInfo* SemanticModel::findField(SymbolId owner, const std::string& name) const {
    if (!structs_.count(owner)) {
        return nullptr;
    }
    const auto& info = structs_.at(owner);
    if (!info.field_name.count(name)) {
        return nullptr;
    }
    auto field_id = info.field_name.at(name);
    if (field_id >= info.fields.size()) {
        return nullptr;
    }
    return &info.fields[field_id];
}

void SemanticModel::setFunctionBody(FunctionId id, FunctionBodyInfo body) {
    if (id >= functions_.size()) {
        return;
    }
    function_bodies_[id] = body;
}

const FunctionBodyInfo* SemanticModel::findFunctionBody(FunctionId id) const {
    if (!function_bodies_.count(id)) {
        return nullptr;
    }
    return &function_bodies_.at(id);
}

} // namespace semantic
