#pragma once
#include "../ast/ast.hpp"
#include "semantic/semantic_ids.hpp"
#include "types.hpp"
#include "struct_resolver.hpp"
#include "impl_resolver.hpp"
#include "function_resolver.hpp"
#include "body_checker.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace semantic {

class ConstEvaluator;

class SemanticModel {
    friend class FunctionResolver;
    friend class StructResolver;
    friend class LayoutChecker;
    friend class DeriveChecker;
    friend class ImplResolver;
    friend class ConstEvaluator;
public:
    TypeContext& typeContext() { return types_; }
    const TypeContext& typeContext() const { return types_; }

    const FunctionInfo* findFunction(FunctionId id) const;
    std::optional<FunctionId> findFunctionId(const ast::FunctionItem* declaration) const;
    std::optional<FunctionId> findTopLevelFunction(SymbolId symbol) const;
    std::optional<FunctionId> findAssociatedFunction(AssocId associated) const;
    const AssocInfo* findAssociated(SymbolId owner, const std::string& name) const;
    const StructInfo* findStruct(SymbolId id) const;
    const FieldInfo* findField(SymbolId owner, const std::string& name) const;
    void setFunctionBody(FunctionId id, FunctionBodyInfo body);
    const FunctionBodyInfo* findFunctionBody(FunctionId id) const;

private:
    TypeContext types_;
    std::vector<FunctionInfo> functions_;
    std::unordered_map<SymbolId, StructInfo> structs_;
    std::vector<ImplInfo> impls_;
    std::vector<AssocInfo> assocs_;
    std::unordered_map<const ast::FunctionItem*, FunctionId> function_ids_;
    std::unordered_map<SymbolId, FunctionId> top_level_function_ids_;
    std::unordered_map<AssocId, FunctionId> associated_function_ids_;
    std::unordered_map<SymbolId, std::vector<ImplId>> impl_by_struct_;
    std::unordered_map<SymbolId, std::unordered_map<std::string, AssocId>> assoc_by_struct_;
    std::unordered_map<const ast::ImplItem*, ImplId> impl_ids_;
    std::unordered_map<const ast::Item*, AssocId> assoc_ids_;
    std::unordered_map<FunctionId, FunctionBodyInfo> function_bodies_;
};

} // namespace semantic
