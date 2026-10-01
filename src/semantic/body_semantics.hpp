#pragma once
#include "semantic_ids.hpp"
#include "ast/ast.hpp"
#include <string>
#include <unordered_map>

namespace semantic {

enum class ValueCategory {
    Value,
    Place
};

enum class PlaceAccess {
    NotPlace,
    ReadOnly,
    Writable
};

enum class AdjustmentKind {
    Dereference,
    BorrowShared,
    BorrowMutable,
    MutableToShared
};

struct Adjustment {
    AdjustmentKind kind;
    TyId result_type;
};

struct LocalInfo {
    std::string name;
    TyId type;
    bool mutable_binding;
    bool is_parameter;
    uint32_t scope_depth;
    ast::SourceSpan declaration;
};

struct ExprSemantics {
    TyId type;
    ValueCategory category;
    PlaceAccess access;
    std::optional<LocalId> local;
    std::optional<SymbolId> symbol;
    std::optional<FunctionId> function;
    std::optional<size_t> field_ordinal;
    std::vector<Adjustment> adjustments;
};

struct FunctionBodyInfo {
    std::vector<LocalInfo> locals;
    std::unordered_map<const ast::Expression*, ExprSemantics> expressions;
    std::unordered_map<const ast::LetStatement*, LocalId> let_statements;
    std::unordered_map<const ast::BreakExpression*, LoopId> break_targets;
    std::unordered_map<const ast::ContinueExpression*, LoopId> continue_targets;
};

} // namespace semantic