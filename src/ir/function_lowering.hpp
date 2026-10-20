#pragma once
#include "ast/ast.hpp"
#include "ir/builder.hpp"
#include "ir/function.hpp"
#include "ir/instruction.hpp"
#include "ir/ir_ids.hpp"
#include "ir/layout.hpp"
#include "ir/module_lowering.hpp"
#include "semantic/body_semantics.hpp"
#include "semantic/function_resolver.hpp"
#include "semantic/path_resolution.hpp"
#include "semantic/semantic_ids.hpp"
#include "semantic/semantic_model.hpp"
#include "semantic/types.hpp"
#include <vector>
#include <optional>

namespace ir {

using FunctionMap = std::map<semantic::FunctionId, ir::FunctionId>;
class TypeLowering;

struct LocalSlot {
    Operand address;
    TypeId storage_type;
    uint8_t alignment;
};

struct BodyResult {
    bool reaches_end;
    std::optional<Operand> value;
};

struct Place {
    Operand address;
    semantic::TyId type;
    uint8_t alignment;
};

struct PlaceCursor {
    semantic::TyId type;
    std::variant<Operand, Place> location;
};

enum class BaseMode {
    Effective,
    Raw
};

struct LoopContext {
    BlockId continue_target;
    std::optional<BlockId> exit;
    std::optional<Place> result_slot;
};

struct VecSlots {
    Operand data;
    Operand length;
    Operand capacity;
};

class FunctionLowering {
public:
    FunctionLowering(const semantic::SemanticModel& model, const semantic::FunctionInfo& function, const semantic::FunctionBodyInfo& body, Module& module, TypeLowering& types, TargetLayout& layout, const FunctionMap& functions, const RuntimeFunctions& runtime):
        model_(model), function_(function), body_(body), module_(module), types_(types), layout_(layout), functions_(functions), builder_(module), locals_(body.locals.size()), runtime_(runtime) {}
    void lower();
private:
    const semantic::SemanticModel& model_;
    const semantic::FunctionInfo& function_;
    const semantic::FunctionBodyInfo& body_;
    Module& module_;
    TypeLowering& types_;
    TargetLayout& layout_;
    const FunctionMap& functions_;
    IrBuilder builder_;
    FunctionId ir_function_;
    std::vector<std::optional<LocalSlot>> locals_;
    std::map<semantic::LoopId, LoopContext> loops_;
    const RuntimeFunctions& runtime_;

    LocalSlot& ensureLocalSlot(semantic::LocalId id);
    const LocalSlot& localSlot(semantic::LocalId id) const;
    Operand loadLocal(semantic::LocalId id);
    void storeLocal(semantic::LocalId id, Operand value);
    void bindParameters();
    BodyResult lowerBody();
    void finishReturn(const BodyResult& result);

    BodyResult lowerExpr(const ast::Expression& expression);
    BodyResult lowerExprRaw(const ast::Expression& expression);
    BodyResult lowerBlock(const ast::BlockExpression& block);
    bool lowerStatement(const ast::Statement& statement);
    bool lowerLet(const ast::LetStatement& statement);

    BodyResult lowerUnary(const ast::UnaryExpression& expression);
    BodyResult lowerBinary(const ast::BinaryExpression& expression);
    BodyResult lowerCall(const ast::CallExpression& expression);
    BodyResult lowerReturn(const ast::ReturnExpression& expression);
    semantic::PrimaryTyKind requireScalarOperand(const semantic::OperandPlan& plan) const;
    Operand readScalarOperand(Operand evaluated, const semantic::OperandPlan& plan);

    Operand loadPlace(const Place& place);
    void storePlace(const Place& place, Operand value);
    Place createTemporary(semantic::TyId type);
    std::optional<Place> lowerPlace(const ast::Expression& expression);
    std::optional<PlaceCursor> lowerPlaceBase(const ast::Expression& expression, BaseMode mode);
    std::optional<Place> applyPlaceSteps(PlaceCursor cursor, const std::vector<semantic::PlaceStep>& steps);

    BodyResult lowerCast(const ast::CastExpression& expression);
    BodyResult lowerAssignment(const ast::AssignmentExpression& expression);
    Operand emitCompoundOperation(ast::AssignmentOperator op, Operand lhs, Operand rhs, semantic::PrimaryTyKind left_kind);

    BodyResult lowerIf(const ast::IfExpression& expression);
    BodyResult lowerShortCircuit(const ast::BinaryExpression& expression);
    BodyResult lowerLoop(const ast::LoopExpression& expression);
    BodyResult lowerWhile(const ast::WhileExpression& expression);
    BodyResult lowerBreak(const ast::BreakExpression& expression);
    BodyResult lowerContinue(const ast::ContinueExpression& expression);

    BodyResult lowerArray(const ast::ArrayExpression& expression);
    BodyResult lowerStruct(const ast::StructExpression& expression);
    Place arrayElementPlace(const Place& array, Operand index);
    Place structFieldPlace(const Place& object, size_t ordinal);
    void fillRepeatedArray(const Place& array, Operand element, uint32_t length);

    BodyResult lowerMethodCall(const ast::MethodCallExpression& expression);
    BodyResult lowerResolvedCall(const semantic::CallPlan& plan, const std::vector<ast::AstPtr<ast::Expression>>& arguments);
    BodyResult lowerReceiver(const semantic::ReceiverPlan& plan);

    TypeLayout objectLayout(semantic::TyId type);
    Operand emitAllocate(semantic::TyId element_type, Operand count);
    VecSlots vecSlots(semantic::TyId owner, Operand address);
    Place vectorElementPlace(semantic::TyId element_type, Operand data, Operand index);
    Operand emitArrayLen(semantic::TyId owner);
    Operand emitBoxNew(semantic::TyId owner, Operand value);
    Operand emitVecNew(semantic::TyId owner);
    Operand emitVecLen(semantic::TyId owner, Operand receiver);
    Operand emitVecIsEmpty(semantic::TyId owner, Operand receiver);
    void emitVecPush(semantic::TyId owner, Operand receiver, Operand value);
    Operand emitVecRemove(semantic::TyId owner, Operand receiver, Operand index);
    BodyResult lowerBuiltin(const semantic::BuiltinTarget& target, const std::vector<Operand>& arguments);
};

} // namespace ir