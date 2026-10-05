#pragma once
#include "semantic_ids.hpp"
#include "ast/ast.hpp"
#include <string>
#include <unordered_map>

namespace semantic {

class SemanticModel;

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

struct PlaceInfo {
    bool writable_here = false;
    bool crossed_shared_reference = false;
    bool blocked_by_vec_access = false;
};

enum class PlaceStepKind {
    Materialize,
    DerefShared,
    DerefMutable,
    DerefBox,
    Field,
    ArrayIndex,
    VecIndex
};

struct PlaceStep {
    PlaceStepKind kind;
    TyId result_type;
    std::optional<size_t> field_ordinal;
    const ast::Expression* index_expression = nullptr;
};

enum class CoercionKind {
    Identity,
    NeverToAny,
    BorrowShared,
    BorrowMutable,
    Recovery
};

struct CoercionPlan {
    TyId source_kind;
    TyId target_type;
    CoercionKind kind;
    std::vector<PlaceStep> place_steps;
    std::optional<PlaceInfo> reference_access;
};

struct ExprSemantics {
    TyId type;
    std::optional<TyId> coerced_type;
    ValueCategory category;
    PlaceAccess access;
    std::optional<PlaceInfo> place;
    std::optional<LocalId> local;
    std::optional<SymbolId> symbol;
    std::optional<FunctionId> function;
    std::optional<size_t> field_ordinal;
    std::vector<Adjustment> adjustments;

    TyId effectiveType() const;
};

struct FunctionBodyInfo {
    std::vector<LocalInfo> locals;
    std::vector<LocalId> parameter_locals;
    std::unordered_map<const ast::Expression*, ExprSemantics> expressions;
    std::unordered_map<const ast::LetStatement*, LocalId> let_statements;
    std::unordered_map<const ast::BreakExpression*, LoopId> break_targets;
    std::unordered_map<const ast::ContinueExpression*, LoopId> continue_targets;
    std::unordered_map<const ast::Expression*, LoopId> loop_ids;
    std::unordered_map<const ast::Expression*, CoercionPlan> coercions;
};

TyId effectiveType(const ExprSemantics& info);

bool isInteger(TyId type, SemanticModel& model);
bool isSignedInteger(TyId type, SemanticModel& model);
bool isBool(TyId type, SemanticModel& model);
bool isNever(TyId type, SemanticModel& model);
bool isError(TyId type, SemanticModel& model);

} // namespace semantic