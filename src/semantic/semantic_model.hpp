#pragma once
#include "../ast/ast.hpp"
#include "symbol.hpp"
#include "types.hpp"
#include "struct_resolver.hpp"
#include "impl_resolver.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>

namespace semantic {

class ConstEvaluator;
using ConstantValue = std::variant<bool, int32_t, uint32_t, int64_t, uint64_t>;

class SemanticModel {
    friend class StructResolver;
    friend class LayoutChecker;
    friend class DeriveChecker;
    friend class ImplResolver;
    friend class ConstEvaluator;
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
    std::unordered_map<SymbolId, std::unordered_map<std::string, AssocId>> assoc_by_struct_;
    std::unordered_map<const ast::ImplItem*, ImplId> impl_ids_;
    std::unordered_map<const ast::Item*, AssocId> assoc_ids_;
};

} // namespace semantic
