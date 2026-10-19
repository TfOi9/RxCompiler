#pragma once
#include "ast/ast.hpp"
#include "ir/builder.hpp"
#include "ir/function.hpp"
#include "ir/instruction.hpp"
#include "ir/ir_ids.hpp"
#include "ir/layout.hpp"
#include "semantic/body_semantics.hpp"
#include "semantic/function_resolver.hpp"
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

class FunctionLowering {
public:
    FunctionLowering(const semantic::SemanticModel& model, const semantic::FunctionInfo& function, const semantic::FunctionBodyInfo& body, Module& module, TypeLowering& types, TargetLayout& layout, const FunctionMap& functions):
        model_(model), function_(function), body_(body), module_(module), types_(types), layout_(layout), functions_(functions), builder_(module), locals_(body.locals.size()) {}
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

    Operand loadPlace(const Place& place);
    void storePlace(const Place& place, Operand value);
    Place createTemporary(semantic::TyId type);
    std::optional<Place> lowerPlace(const ast::Expression& expression);
    std::optional<PlaceCursor> lowerPlaceBase(const ast::Expression& expression, BaseMode mode);
    std::optional<Place> applyPlaceSteps(PlaceCursor cursor, const std::vector<semantic::PlaceStep>& steps);

    BodyResult lowerCast(const ast::CastExpression& expression);
    Operand readScalarOperand(Operand evaluated, const semantic::OperandPlan& plan);
};

} // namespace ir