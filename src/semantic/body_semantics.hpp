#pragma once
#include "semantic/const_value.hpp"
#include "semantic_ids.hpp"
#include "path_resolution.hpp"
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

enum class OperandReadKind {
    Value,
    SharedReferent,
    EqualityBorrow,
    OrderedReferent
};

struct OperandPlan {
    TyId source_type;
    TyId operation_type;
    OperandReadKind read_kind;
    bool outer_mutable_to_shared = false;
    size_t reference_depth = 0;
};

struct UnaryPlan {
    OperandPlan operand;
    TyId result_type;
};

struct BinaryPlan {
    OperandPlan left;
    OperandPlan right;
    TyId result_type;
};

struct PlacePlan {
    const ast::Expression* base;
    std::vector<PlaceStep> steps;
};

using CallTarget = std::variant<FunctionTarget, BuiltinTarget>;

enum class ReceiverAction {
    Value,
    BorrowShared,
    BorrowMutable
};

struct ReceiverPlan {
    const ast::Expression* expression;
    TyId source_type;
    TyId parameter_type;
    std::vector<PlaceStep> steps;
    ReceiverAction action;
};

struct CallPlan {
    CallTarget target;
    std::optional<ReceiverPlan> receiver;
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
    std::unordered_map<const ast::Expression*, EvaluatedConst> constant_values;
    std::unordered_map<const ast::Expression*, UnaryPlan> unary_operations;
    std::unordered_map<const ast::Expression*, BinaryPlan> binary_operations;
    std::unordered_map<const ast::Expression*, PlacePlan> place_operations;
    std::unordered_map<const ast::Expression*, CallPlan> calls;
};

TyId effectiveType(const ExprSemantics& info);

bool isInteger(TyId type, SemanticModel& model);
bool isSignedInteger(TyId type, SemanticModel& model);
bool isBool(TyId type, SemanticModel& model);
bool isNever(TyId type, SemanticModel& model);
bool isError(TyId type, SemanticModel& model);

} // namespace semantic