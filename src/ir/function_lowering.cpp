#include "ir/function_lowering.hpp"
#include "ast/ast.hpp"
#include "ir/builder.hpp"
#include "ir/instruction.hpp"
#include "ir/ir_ids.hpp"
#include "ir/layout.hpp"
#include "ir/module_lowering.hpp"
#include "semantic/body_semantics.hpp"
#include "semantic/integer_literal.hpp"
#include "semantic/path_resolution.hpp"
#include "semantic/semantic_ids.hpp"
#include "semantic/types.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <variant>

namespace ir {

LocalSlot& FunctionLowering::ensureLocalSlot(semantic::LocalId id) {
    IrBuilder::requireIr(id < locals_.size(), "local ID out of range");
    if (locals_[id].has_value()) {
        return *locals_[id];
    }
    const auto& info = body_.locals.at(id);
    TypeId storage = types_.storageType(info.type);
    TypeLayout object_layout = layout_.layoutOf(storage);
    IrBuilder::requireIr(object_layout.alignment <= std::numeric_limits<uint8_t>::max(), "local alignment exceeds IR representation");
    auto alignment = static_cast<uint8_t>(object_layout.alignment);
    Operand address = builder_.emitEntryAlloca(storage, alignment);
    locals_[id].emplace(LocalSlot{address, storage, alignment});
    return *locals_[id];
}

const LocalSlot& FunctionLowering::localSlot(semantic::LocalId id) const {
    IrBuilder::requireIr(id < locals_.size(), "local ID out of range");
    IrBuilder::requireIr(locals_[id].has_value(), "local storage has not been generated");
    return *locals_[id];
}

Operand FunctionLowering::loadLocal(semantic::LocalId id) {
    const LocalSlot& slot = localSlot(id);
    TypeId value_type = types_.valueType(body_.locals.at(id).type);
    Operand value = builder_.emitLoad(slot.storage_type, slot.address, slot.alignment);
    if (slot.storage_type == value_type) {
        return value;
    }
    IrBuilder::requireIr(slot.storage_type == module_.types.i8() && value_type == module_.types.i1(), "unsupported local value conversion");
    return builder_.emitTrunc(value, value_type);
}

void FunctionLowering::storeLocal(semantic::LocalId id, Operand value) {
    const LocalSlot& slot = localSlot(id);
    TypeId value_type = types_.valueType(body_.locals.at(id).type);
    IrBuilder::requireIr(value_type == value.type, "local storage type mismatch");
    if (slot.storage_type != value_type) {
        IrBuilder::requireIr(value_type == module_.types.i1() && slot.storage_type == module_.types.i8(), "unsupported local store conversion");
        value = builder_.emitZExt(value, slot.storage_type);
    }
    builder_.emitStore(value, slot.address, slot.alignment);
}

void FunctionLowering::bindParameters() {
    const auto& params = function_.signature.parameters;
    IrBuilder::requireIr(body_.parameter_locals.size() == params.size(), "parameter local count mismatch");
    for (size_t i = 0; i < params.size(); i++) {
        semantic::LocalId local = body_.parameter_locals[i];
        ensureLocalSlot(local);
        Operand incoming = builder_.parameter(ir_function_, i);
        storeLocal(local, incoming);
    }
}

void FunctionLowering::finishReturn(const BodyResult& result) {
    if (!result.reaches_end) {
        IrBuilder::requireIr(!result.value.has_value(), "non-fallthrough body has a value");
        IrBuilder::requireIr(!builder_.hasInsertionPoint(), "non-fallthrough body has an open block");
        return;
    }
    IrBuilder::requireIr(builder_.hasInsertionPoint(), "fallthrough body has no insertion point");
    TypeId return_type = types_.returnType(function_.signature.return_type);
    if (return_type == module_.types.none()) {
        IrBuilder::requireIr(!result.value.has_value(), "unit function body has an IR value");
        builder_.emitRet(std::nullopt);
        return;    
    }
    IrBuilder::requireIr(result.value.has_value(), "non-unit function body has no return value");
    builder_.emitRet(result.value);
}

void FunctionLowering::lower() {
    auto it = functions_.find(function_.id);
    IrBuilder::requireIr(it != functions_.end(), "semantic function has no IR mapping");
    ir_function_ = it->second;
    auto entry = builder_.appendBlock(ir_function_);
    builder_.positionAtEnd(ir_function_, entry);
    bindParameters();
    BodyResult result = lowerBody();
    finishReturn(result);
    builder_.clearInsertionPoint();
}

BodyResult FunctionLowering::lowerBody() {
    IrBuilder::requireIr(function_.declaration != nullptr && function_.declaration->body != nullptr, "source function has no body");
    return lowerExpr(*function_.declaration->body);
}

BodyResult FunctionLowering::lowerBlock(const ast::BlockExpression& block) {
    for (const auto& statement: block.statements) {
        if (!lowerStatement(*statement)) {
            return {false, std::nullopt};
        }
    }
    if (block.tail_expression) {
        return lowerExpr(*block.tail_expression);
    }
    return {true, std::nullopt};
}

BodyResult FunctionLowering::lowerExpr(const ast::Expression& expression) {
    IrBuilder::requireIr(builder_.hasInsertionPoint(), "expression has no insertion point");
    auto info_it = body_.expressions.find(&expression);
    IrBuilder::requireIr(info_it != body_.expressions.end(), "expression has no semantic information");
    const auto& info = info_it->second;
    const semantic::CoercionPlan* conversion = nullptr;
    if (auto it = body_.coercions.find(&expression); it != body_.coercions.end()) {
        conversion = &it->second;
    }
    BodyResult result = {false, std::nullopt};
    if (conversion == nullptr) {
        result = lowerExprRaw(expression);
    } else {
        switch (conversion->kind) {
            case semantic::CoercionKind::Identity:
            case semantic::CoercionKind::NeverToAny:
                result = lowerExprRaw(expression);
                break;
            case semantic::CoercionKind::BorrowShared:
            case semantic::CoercionKind::BorrowMutable: {
                auto cursor = lowerPlaceBase(expression, BaseMode::Raw);
                if (!cursor) {
                    result = {false, std::nullopt};
                    break;
                }
                auto place = applyPlaceSteps(std::move(*cursor), conversion->place_steps);
                if (!place) {
                    result = {false, std::nullopt};
                    break;
                }
                result = {true, place->address};
                break;
            }
            case semantic::CoercionKind::Recovery:
                throw std::logic_error("cannot lower a recovery coercion");
            default:
                throw std::logic_error("unknown coercion kind");
        }
    }
    if (conversion && conversion->kind == semantic::CoercionKind::NeverToAny) {
        IrBuilder::requireIr(!result.reaches_end, "never expression completed normally");
    }
    IrBuilder::requireIr(result.reaches_end == builder_.hasInsertionPoint(), "expression control-flow state mismatch");
    if (!result.reaches_end) {
        IrBuilder::requireIr(!result.value.has_value(), "non-fallthrough expression has a value");
        return result;
    }
    const auto& type = model_.typeContext().get(info.effectiveType());
    IrBuilder::requireIr(!std::holds_alternative<semantic::ErrorTy>(type) && !std::holds_alternative<semantic::NeverTy>(type), "fallthrough expression has an invalid result type");
    if (std::holds_alternative<semantic::UnitTy>(type)) {
        IrBuilder::requireIr(!result.value.has_value(), "unit expression has an IR value");
    } else {
        IrBuilder::requireIr(result.value.has_value(), "non-unit expression has no IR value");
        IrBuilder::requireIr(result.value->type == types_.valueType(info.effectiveType()), "expression result type mismatch");
    }
    return result;
}

BodyResult FunctionLowering::lowerExprRaw(const ast::Expression& expression) {
    switch (expression.type) {
        case ast::NodeType::IntegerExpr: {
            const auto& integer = static_cast<const ast::IntegerExpression&>(expression);
            uint64_t magnitude = 0;
            std::string error;
            if (!semantic::parseIntegerMagnitude(integer.value, magnitude, error)) {
                throw std::logic_error(error);
            }
            const auto& info = body_.expressions.at(&expression);
            Operand value = builder_.constantInt(types_.valueType(info.type), static_cast<uint32_t>(magnitude));
            return {true, value};
        }
        case ast::NodeType::BoolExpr: {
            const auto& boolean = static_cast<const ast::BoolExpression&>(expression);
            return {
                true,
                builder_.constantInt(module_.types.i1(), boolean.value ? 1u : 0u)
            };
        }
        case ast::NodeType::UnitExpr:
            return {true, std::nullopt};
        case ast::NodeType::PathExpr: {
            const auto& info = body_.expressions.at(&expression);
            if (info.local.has_value()) {
                return {true, loadLocal(*info.local)};
            }
            auto it = body_.constant_values.find(&expression);
            IrBuilder::requireIr(it != body_.constant_values.end(), "path has no local or constant value");
            const auto& constant = it->second;
            uint32_t bits = std::visit(
                [](auto value) -> uint32_t {
                    return static_cast<uint32_t>(value);
                },
                constant.value
            );
            return {
                true,
                builder_.constantInt(types_.valueType(constant.type), bits)
            };
        }
        case ast::NodeType::GroupedExpr: {
            const auto& grouped = static_cast<const ast::GroupedExpression&>(expression);
            IrBuilder::requireIr(grouped.expr != nullptr, "grouped expression has no operand");
            return lowerExpr(*grouped.expr);
        }
        case ast::NodeType::BlockExpr:
            return lowerBlock(static_cast<const ast::BlockExpression&>(expression));
        case ast::NodeType::UnaryExpr:
            return lowerUnary(static_cast<const ast::UnaryExpression&>(expression));
        case ast::NodeType::BinaryExpr:
            return lowerBinary(static_cast<const ast::BinaryExpression&>(expression));
        case ast::NodeType::CallExpr:
            return lowerCall(static_cast<const ast::CallExpression&>(expression));
        case ast::NodeType::ReturnExpr:
            return lowerReturn(static_cast<const ast::ReturnExpression&>(expression));
        case ast::NodeType::FieldExpr:
        case ast::NodeType::IndexExpr: {
            auto place = lowerPlace(expression);
            if (!place) {
                return {false, std::nullopt};
            }
            return {true, loadPlace(*place)};
        }
        case ast::NodeType::CastExpr:
            return lowerCast(static_cast<const ast::CastExpression&>(expression));
        case ast::NodeType::AssignExpr:
            return lowerAssignment(static_cast<const ast::AssignmentExpression&>(expression));
        case ast::NodeType::IfExpr:
            return lowerIf(static_cast<const ast::IfExpression&>(expression));
        default:
            // TODO unimplemented
            throw std::logic_error("expression lowering unimplemented");
    }
}

bool FunctionLowering::lowerStatement(const ast::Statement& statement) {
    switch (statement.type) {
        case ast::NodeType::EmptyStmt:
            return true;
        case ast::NodeType::LetStmt:
            return lowerLet(static_cast<const ast::LetStatement&>(statement));
        case ast::NodeType::ExprStmt: {
            const auto& expression = static_cast<const ast::ExpressionStatement&>(statement);
            IrBuilder::requireIr(expression.expr != nullptr, "expression statement has no expression");
            BodyResult result = lowerExpr(*expression.expr);
            return result.reaches_end;
        }
        // TODO unimplemented
        default:
            throw std::logic_error("unexpected statement type");
    }
}

bool FunctionLowering::lowerLet(const ast::LetStatement& statement) {
    auto it = body_.let_statements.find(&statement);
    IrBuilder::requireIr(it != body_.let_statements.end(), "let statement has no semantic local");
    const semantic::LocalId local = it->second;
    IrBuilder::requireIr(statement.expression != nullptr, "let statement has no initializer");
    BodyResult initializer = lowerExpr(*statement.expression);
    if (!initializer.reaches_end) {
        return false;
    }
    IrBuilder::requireIr(initializer.value.has_value(), "let initializer has no runtime value");
    ensureLocalSlot(local);
    storeLocal(local, *initializer.value);
    return true;
}

BodyResult FunctionLowering::lowerCall(const ast::CallExpression& expression) {
    auto plan_it = body_.calls.find(&expression);
    IrBuilder::requireIr(plan_it != body_.calls.end(), "call expression has no semantic plan");
    const auto& plan = plan_it->second;
    IrBuilder::requireIr(!plan.receiver.has_value(), "method receiver is not implemented");
    const auto* target = std::get_if<semantic::FunctionTarget>(&plan.target);
    IrBuilder::requireIr(target != nullptr, "builtin operation lowering is not impleemnted");
    auto function_it = functions_.find(target->id);
    IrBuilder::requireIr(function_it != functions_.end(), "callee has no IR function mapping");
    std::vector<Operand> args;
    args.reserve(expression.args.size());
    for (const auto& arg: expression.args) {
        IrBuilder::requireIr(arg != nullptr, "call argument is empty");
        BodyResult result = lowerExpr(*arg);
        if (!result.reaches_end) {
            return result;
        }
        IrBuilder::requireIr(result.value.has_value(), "call argument has no runtime value");
        args.push_back(*result.value);
    }
    return {
        true,
        builder_.emitCall(function_it->second, args)
    };
}

BodyResult FunctionLowering::lowerReturn(const ast::ReturnExpression& expression) {
    std::optional<Operand> value;
    if (expression.expr) {
        BodyResult result = lowerExpr(*expression.expr);
        if (!result.reaches_end) {
            return result;
        }
        value = result.value;
    }
    builder_.emitRet(value);
    builder_.clearInsertionPoint();
    return {false, std::nullopt};
}

semantic::PrimaryTyKind FunctionLowering::requireScalarOperand(const semantic::OperandPlan& plan) const {
    const auto& type = model_.typeContext().get(plan.operation_type);
    const auto* primary = std::get_if<semantic::PrimaryTy>(&type);
    IrBuilder::requireIr(primary != nullptr, "non-primitive operand lowering is not implemented");
    switch (plan.read_kind) {
        case semantic::OperandReadKind::Value:
        case semantic::OperandReadKind::EqualityBorrow:
            IrBuilder::requireIr(plan.reference_depth == 0, "direct scalar operand has reference depth");
            break;
        case semantic::OperandReadKind::SharedReferent: {
            const auto* reference = std::get_if<semantic::RefTy>(&model_.typeContext().get(plan.source_type));
            IrBuilder::requireIr(plan.reference_depth == 1 && reference != nullptr && !reference->is_mut, "shared scalar read requires one shared reference");
            break;
        }
        case semantic::OperandReadKind::OrderedReferent: {
            IrBuilder::requireIr(plan.reference_depth > 0, "ordered referent read has no reference");
            break;
        }
        default:
            throw std::logic_error("unknown operand read kind");
    }
    if (plan.outer_mutable_to_shared) {
        const auto* reference = std::get_if<semantic::RefTy>(&model_.typeContext().get(plan.source_type));
        IrBuilder::requireIr(plan.read_kind == semantic::OperandReadKind::OrderedReferent && reference != nullptr && reference->is_mut, "invalide outer reference adjustment");
    }
    semantic::TyId current = plan.source_type;
    for (size_t i = 0; i < plan.reference_depth; i++) {
        const auto* reference = std::get_if<semantic::RefTy>(&model_.typeContext().get(current));
        IrBuilder::requireIr(reference != nullptr, "operand reference depth exceeds its reference chain");
        current = reference->target;
    }
    IrBuilder::requireIr(current == plan.operation_type, "operand read does not reach its operation type");
    return primary->ty;
}

Operand FunctionLowering::readScalarOperand(Operand evaluated, const semantic::OperandPlan& plan) {
    IrBuilder::requireIr(evaluated.type == types_.valueType(plan.source_type), "evaluated type does match its plan");
    Operand value = evaluated;
    semantic::TyId current = plan.source_type;
    for (size_t i = 0; i < plan.reference_depth; i++) {
        const auto* reference = std::get_if<semantic::RefTy>(&model_.typeContext().get(current));
        const semantic::TyId target = reference->target;
        const auto object_layout = layout_.layoutOf(types_.storageType(target));
        IrBuilder::requireIr(object_layout.alignment <= std::numeric_limits<uint8_t>::max(), "referent alignment exceeds IR representations");
        value = loadPlace(Place{value, target, static_cast<uint8_t>(object_layout.alignment)});
        current = target;
    }
    IrBuilder::requireIr(value.type == types_.valueType(plan.operation_type), "scalar operand result type mismatch");
    return value;
}

BodyResult FunctionLowering::lowerUnary(const ast::UnaryExpression& expression) {
    if (expression.op == ast::UnaryOperator::Dereference) {
        auto place = lowerPlace(expression);
        if (!place) {
            return {false, std::nullopt};
        }
        return {true, loadPlace(*place)};
    }
    if (expression.op == ast::UnaryOperator::Borrow || expression.op == ast::UnaryOperator::BorrowMut) {
        const auto& plan = body_.place_operations.at(&expression);
        auto cursor = lowerPlaceBase(*plan.base, BaseMode::Effective);
        if (!cursor) {
            return {false, std::nullopt};
        }
        auto place = applyPlaceSteps(std::move(*cursor), plan.steps);
        if (!place) {
            return {false, std::nullopt};
        }
        return {true, place->address};
    }
    auto plan_it = body_.unary_operations.find(&expression);
    IrBuilder::requireIr(plan_it != body_.unary_operations.end(), "unary expression has no semantic plan");
    const auto& plan = plan_it->second;
    auto kind = requireScalarOperand(plan.operand);
    IrBuilder::requireIr(expression.operand != nullptr, "unary expression has no operand");
    BodyResult result = lowerExpr(*expression.operand);
    if (!result.reaches_end) {
        return result;
    }
    IrBuilder::requireIr(result.value.has_value(), "unary operand has no runtime value");
    Operand value = readScalarOperand(*result.value, plan.operand);
    switch (expression.op) {
        case ast::UnaryOperator::Negation:
            return {
                true,
                builder_.emitBinary(OpCode::Sub, builder_.constantInt(value.type, 0), value)
            };
        case ast::UnaryOperator::Not: {
            uint32_t mask = kind == semantic::PrimaryTyKind::Bool ? 1u : std::numeric_limits<uint32_t>::max();
            return {
                true,
                builder_.emitBinary(OpCode::Xor, value, builder_.constantInt(value.type, mask))
            };
        }
        default:
            // TODO unimplemented
            throw std::logic_error("unexpected unary operantor");
    }
}

BodyResult FunctionLowering::lowerBinary(const ast::BinaryExpression& expression) {
    if (expression.op == ast::BinaryOperator::LogicalAnd || expression.op == ast::BinaryOperator::LogicalOr) {
        return lowerShortCircuit(expression);
    }
    auto plan_it = body_.binary_operations.find(&expression);
    IrBuilder::requireIr(plan_it != body_.binary_operations.end(), "binary expression has no semantic plan");
    const auto& plan = plan_it->second;
    auto left_kind = requireScalarOperand(plan.left);
    requireScalarOperand(plan.right);
    bool signed_left = semantic::isSignedKind(left_kind);
    IrBuilder::requireIr(expression.lhs_operand != nullptr && expression.rhs_operand != nullptr, "binary expression has empty operand");
    BodyResult left = lowerExpr(*expression.lhs_operand);
    if (!left.reaches_end) {
        return left;
    }
    IrBuilder::requireIr(left.value.has_value(), "left operand has no runtime value");
    BodyResult right = lowerExpr(*expression.rhs_operand);
    if (!right.reaches_end) {
        return right;
    }
    IrBuilder::requireIr(right.value.has_value(), "right operand has no runtime value");
    Operand lhs = readScalarOperand(*left.value, plan.left);
    Operand rhs = readScalarOperand(*right.value, plan.right);
    auto binary = [&](OpCode opcode) -> BodyResult {
        return {true, builder_.emitBinary(opcode, lhs, rhs)};
    };
    auto compare = [&](Predicate predicate) -> BodyResult {
        return {true, builder_.emitICmp(predicate, lhs, rhs)};
    };
    switch (expression.op) {
        case ast::BinaryOperator::Add:
            return binary(OpCode::Add);
        case ast::BinaryOperator::Subtract:
            return binary(OpCode::Sub);
        case ast::BinaryOperator::Multiply:
            return binary(OpCode::Mul);
        case ast::BinaryOperator::Divide:
            return binary(signed_left ? OpCode::Sdiv : OpCode::Udiv);
        case ast::BinaryOperator::Remainder:
            return binary(signed_left ? OpCode::Srem : OpCode::Urem);
        case ast::BinaryOperator::BitwiseAnd:
            return binary(OpCode::And);
        case ast::BinaryOperator::BitwiseOr:
            return binary(OpCode::Or);
        case ast::BinaryOperator::BitwiseXor:
            return binary(OpCode::Xor);
        case ast::BinaryOperator::ShiftLeft:
        case ast::BinaryOperator::ShiftRight: {
            rhs = builder_.emitBinary(OpCode::And, rhs, builder_.constantInt(rhs.type, 31));
            if (expression.op == ast::BinaryOperator::ShiftLeft) {
                return binary(OpCode::Shl);
            }
            return binary(signed_left ? OpCode::Ashr : OpCode::Lshr);
        }
        case ast::BinaryOperator::Equal:
            return compare(Predicate::Eq);
        case ast::BinaryOperator::NotEqual:
            return compare(Predicate::Ne);
        case ast::BinaryOperator::Less:
            return compare(signed_left ? Predicate::Slt : Predicate::Ult);
        case ast::BinaryOperator::LessEqual:
            return compare(signed_left ? Predicate::Sle : Predicate::Ule);
        case ast::BinaryOperator::Greater:
            return compare(signed_left ? Predicate::Sgt : Predicate::Ugt);
        case ast::BinaryOperator::GreaterEqual:
            return compare(signed_left ? Predicate::Sge : Predicate::Uge);
        default:
            throw std::logic_error("unexpected binary operator");
    }
}

Operand FunctionLowering::loadPlace(const Place& place) {
    TypeId storage_type = types_.storageType(place.type);
    TypeId value_type = types_.valueType(place.type);
    Operand value = builder_.emitLoad(storage_type, place.address, place.alignment);
    if (storage_type == value_type) {
        return value;
    }
    IrBuilder::requireIr(storage_type == module_.types.i8(), "unsupported place load conversion");
    return builder_.emitTrunc(value, value_type);
}

void FunctionLowering::storePlace(const Place& place, Operand value) {
    TypeId storage_type = types_.storageType(place.type);
    TypeId value_type = types_.valueType(place.type);
    IrBuilder::requireIr(value.type == value_type, "place store value type mismatch");
    if (storage_type != value_type) {
        IrBuilder::requireIr(value_type == module_.types.i1() && storage_type == module_.types.i8(), "unsupported place store conversion");
        value = builder_.emitZExt(value, storage_type);
    }
    builder_.emitStore(value, place.address, place.alignment);
}

Place FunctionLowering::createTemporary(semantic::TyId type) {
    TypeId storage_type = types_.storageType(type);
    TypeLayout object_layout = layout_.layoutOf(storage_type);
    IrBuilder::requireIr(object_layout.alignment <= std::numeric_limits<uint8_t>::max(), "temporary alignment exceeds IR representation");
    auto alignment = static_cast<uint8_t>(object_layout.alignment);
    Operand address = builder_.emitEntryAlloca(storage_type, alignment);
    return Place{address, type, alignment};
}

std::optional<Place> FunctionLowering::lowerPlace(const ast::Expression& expression) {
    auto info_it = body_.expressions.find(&expression);
    IrBuilder::requireIr(info_it != body_.expressions.end(), "place expression has no semantic information");
    const auto& info = info_it->second;
    IrBuilder::requireIr(info.category == semantic::ValueCategory::Place, "expression is not a place");
    if (expression.type == ast::NodeType::PathExpr) {
        IrBuilder::requireIr(info.local.has_value(), "path has no local storage");
        const auto id = *info.local;
        const LocalSlot& slot = localSlot(id);
        return Place{
            slot.address,
            body_.locals.at(id).type,
            slot.alignment
        };
    }
    if (expression.type == ast::NodeType::GroupedExpr) {
        const auto& grouped = static_cast<const ast::GroupedExpression&>(expression);
        IrBuilder::requireIr(grouped.expr != nullptr, "grouped place has no expression");
        auto cursor = lowerPlaceBase(*grouped.expr, BaseMode::Effective);
        if (!cursor) {
            return std::nullopt;
        }
        if (const auto* place = std::get_if<Place>(&cursor->location)) {
            return *place;
        }
        Place temp = createTemporary(cursor->type);
        storePlace(temp, std::get<Operand>(cursor->location));
        return temp;
    }
    auto plan_it = body_.place_operations.find(&expression);
    IrBuilder::requireIr(plan_it != body_.place_operations.end(), "place expression has no address plan");
    const auto& plan = plan_it->second;
    IrBuilder::requireIr(plan.base != nullptr, "place plan has no base");
    auto cursor = lowerPlaceBase(*plan.base, BaseMode::Effective);
    if (!cursor) {
        return std::nullopt;
    }
    auto place = applyPlaceSteps(std::move(*cursor), plan.steps);
    if (place) {
        IrBuilder::requireIr(place->type == info.type, "place result type mismatch");
    }
    return place;
}

std::optional<PlaceCursor> FunctionLowering::lowerPlaceBase(const ast::Expression& expression, BaseMode mode) {
    IrBuilder::requireIr(builder_.hasInsertionPoint(), "place base has no insertion point");
    auto info_it = body_.expressions.find(&expression);
    IrBuilder::requireIr(info_it != body_.expressions.end(), "place base has no semantic information");
    const auto& info = info_it->second;
    bool needs_conversion = false;
    if (mode == BaseMode::Effective) {
        auto it = body_.coercions.find(&expression);
        needs_conversion = it != body_.coercions.end() && it->second.kind != semantic::CoercionKind::Identity;
    }
    if (info.category == semantic::ValueCategory::Place && !needs_conversion) {
        auto place = lowerPlace(expression);
        if (!place) {
            return std::nullopt;
        }
        semantic::TyId expected = mode == BaseMode::Raw ? info.type : info.effectiveType();
        IrBuilder::requireIr(place->type == expected, "place base type mismatch");
        return PlaceCursor{place->type, *place};
    }
    BodyResult result = mode == BaseMode::Raw ? lowerExprRaw(expression) : lowerExpr(expression);
    IrBuilder::requireIr(result.reaches_end == builder_.hasInsertionPoint(), "place base control-flow state mismatch");
    if (!result.reaches_end) {
        IrBuilder::requireIr(!result.value.has_value(), "non-fallthrough place base has a value");
        return std::nullopt;
    }
    IrBuilder::requireIr(result.value.has_value(), "place base has no runtime value");
    semantic::TyId type = mode == BaseMode::Raw ? info.type : info.effectiveType();
    IrBuilder::requireIr(result.value->type == types_.valueType(type), "place base value type mismatch");
    return PlaceCursor{type, *result.value};
}

std::optional<Place> FunctionLowering::applyPlaceSteps(PlaceCursor cursor, const std::vector<semantic::PlaceStep>& steps) {
    auto requirePlace = [&]() -> Place {
        const auto* place = std::get_if<Place>(&cursor.location);
        IrBuilder::requireIr(place != nullptr && place->type == cursor.type, "place step requires an address");
        return *place;
    };
    auto alignmentOf = [&](semantic::TyId type) -> uint8_t {
        auto layout = layout_.layoutOf(types_.storageType(type));
        IrBuilder::requireIr(layout.alignment <= std::numeric_limits<uint8_t>::max(), "place alignment exceeds IR representation");
        return static_cast<uint8_t>(layout.alignment);
    };
    auto setPlace = [&](Place place) {
        cursor.type = place.type;
        cursor.location = place;
    };
    for (const auto& step: steps) {
        switch (step.kind) {
            case semantic::PlaceStepKind::Materialize: {
                const auto* value = std::get_if<Operand>(&cursor.location);
                IrBuilder::requireIr(value != nullptr, "materialize requires a value");
                IrBuilder::requireIr(cursor.type == step.result_type, "materialize type mismatch");
                Operand saved = *value;
                Place temp = createTemporary(cursor.type);
                storePlace(temp, saved);
                setPlace(temp);
                break;
            }
            case semantic::PlaceStepKind::DerefShared:
            case semantic::PlaceStepKind::DerefMutable:
            case semantic::PlaceStepKind::DerefBox: {
                Operand pointer;
                if (const auto* place = std::get_if<Place>(&cursor.location)) {
                    pointer = loadPlace(*place);
                } else {
                    pointer = std::get<Operand>(cursor.location);
                }
                IrBuilder::requireIr(pointer.type == module_.types.ptr(), "dereference requires a pointer value");
                setPlace(Place{pointer, step.result_type, alignmentOf(step.result_type)});
                break;
            }
            case semantic::PlaceStepKind::Field: {
                Place base = requirePlace();
                IrBuilder::requireIr(step.field_ordinal.has_value(), "field step has no ordinal");
                IrBuilder::requireIr(*step.field_ordinal <= std::numeric_limits<uint32_t>::max(), "field ordinal exceeds IR representation");
                Operand address = builder_.emitGep(
                    types_.storageType(base.type),
                    base.address,
                    {
                        builder_.constantInt(module_.types.i32(), 0),
                        builder_.constantInt(module_.types.i32(), static_cast<uint32_t>(*step.field_ordinal))
                    }
                );
                setPlace(Place{
                    address,
                    step.result_type,
                    std::min(base.alignment, alignmentOf(step.result_type))
                });
                break;
            }
            case semantic::PlaceStepKind::ArrayIndex:
            case semantic::PlaceStepKind::VecIndex: {
                Place base = requirePlace();
                IrBuilder::requireIr(step.index_expression != nullptr, "index step has no expression");
                BodyResult index = lowerExpr(*step.index_expression);
                if (!index.reaches_end) {
                    return std::nullopt;
                }
                IrBuilder::requireIr(index.value.has_value(), "index expression has no value");
                IrBuilder::requireIr(index.value->type == module_.types.i32(), "index must be usize type");
                Operand address;
                uint8_t alignment;
                if (step.kind == semantic::PlaceStepKind::ArrayIndex) {
                    address = builder_.emitGep(
                        types_.storageType(base.type),
                        base.address,
                        {
                            builder_.constantInt(module_.types.i32(), 0),
                            *index.value
                        }
                    );
                    alignment = std::min(base.alignment, alignmentOf(step.result_type));
                } else {
                    Operand data_slot = builder_.emitGep(
                        types_.storageType(base.type),
                        base.address,
                        {
                            builder_.constantInt(module_.types.i32(), 0),
                            builder_.constantInt(module_.types.i32(), 0)
                        }
                    );
                    auto pointer_layout = layout_.layoutOf(module_.types.ptr());
                    auto data_slot_alignment = static_cast<uint8_t>(std::min<uint32_t>(base.alignment, pointer_layout.alignment));
                    Operand data = builder_.emitLoad(module_.types.ptr(), data_slot, data_slot_alignment);
                    address = builder_.emitGep(types_.storageType(step.result_type), data, {*index.value});
                    alignment = alignmentOf(step.result_type);
                }
                setPlace(Place{
                    address,
                    step.result_type,
                    alignment
                });
                break;
            }
            default:
                throw std::logic_error("unknown place step");
        }
    }
    const auto* place = std::get_if<Place>(&cursor.location);
    IrBuilder::requireIr(place != nullptr && place->type == cursor.type, "place steps must end at an address");
    return *place;
}

BodyResult FunctionLowering::lowerCast(const ast::CastExpression& expression) {
    IrBuilder::requireIr(expression.operand != nullptr, "cast has no operand");
    BodyResult result = lowerExpr(*expression.operand);
    if (!result.reaches_end) {
        return result;
    }
    IrBuilder::requireIr(result.value.has_value(), "cast operand has no runtime value");
    const auto& info = body_.expressions.at(&expression);
    TypeId target = types_.valueType(info.type);
    IrBuilder::requireIr(target == module_.types.i32(), "cast target must lower to i32");
    Operand value = *result.value;
    if (value.type == target) {
        return {true, value};
    }
    IrBuilder::requireIr(value.type == module_.types.i1(), "unsupported cast operand");
    return {true, builder_.emitZExt(value, target)};
}

BodyResult FunctionLowering::lowerAssignment(const ast::AssignmentExpression& expression) {
    IrBuilder::requireIr(expression.lhs_operand != nullptr && expression.rhs_operand != nullptr, "assignment has an empty operand");
    BodyResult right = lowerExpr(*expression.rhs_operand);
    if (!right.reaches_end) {
        return right;
    }
    IrBuilder::requireIr(right.value.has_value(), "assignment right operand has no runtime value");
    auto destination = lowerPlace(*expression.lhs_operand);
    if (!destination) {
        return {false, std::nullopt};
    }
    if (expression.op == ast::AssignmentOperator::Assign) {
        storePlace(*destination, *right.value);
        return {true, std::nullopt};
    }
    auto plan_it = body_.binary_operations.find(&expression);
    IrBuilder::requireIr(plan_it != body_.binary_operations.end(), "compound assignment has no semantic plan");
    const auto& plan = plan_it->second;
    auto left_kind = requireScalarOperand(plan.left);
    requireScalarOperand(plan.right);
    IrBuilder::requireIr(destination->type == plan.left.source_type, "compound assignment destination type mismatch");
    Operand lhs = loadPlace(*destination);
    Operand rhs = readScalarOperand(*right.value, plan.right);
    Operand updated = emitCompoundOperation(expression.op, lhs, rhs, left_kind);
    storePlace(*destination, updated);
    return {true, std::nullopt};
}

Operand FunctionLowering::emitCompoundOperation(ast::AssignmentOperator op, Operand lhs, Operand rhs, semantic::PrimaryTyKind left_kind) {
    bool signed_left = semantic::isSignedKind(left_kind);
    switch (op) {
        case ast::AssignmentOperator::AssignAdd:
            return builder_.emitBinary(OpCode::Add, lhs, rhs);
        case ast::AssignmentOperator::AssignSubtract:
            return builder_.emitBinary(OpCode::Sub, lhs, rhs);
        case ast::AssignmentOperator::AssignMultiply:
            return builder_.emitBinary(OpCode::Mul, lhs, rhs);
        case ast::AssignmentOperator::AssignDivide:
            return builder_.emitBinary(signed_left ? OpCode::Sdiv : OpCode::Udiv, lhs, rhs);
        case ast::AssignmentOperator::AssignRemainder:
            return builder_.emitBinary(signed_left ? OpCode::Srem : OpCode::Urem, lhs, rhs);
        case ast::AssignmentOperator::AssignShiftLeft:
            rhs = builder_.emitBinary(OpCode::And, rhs, builder_.constantInt(rhs.type, 31));
            return builder_.emitBinary(OpCode::Shl, lhs, rhs);
        case ast::AssignmentOperator::AssignShiftRight:
            rhs = builder_.emitBinary(OpCode::And, rhs, builder_.constantInt(rhs.type, 31));
            return builder_.emitBinary(signed_left ? OpCode::Ashr : OpCode::Lshr, lhs, rhs);
        case ast::AssignmentOperator::AssignBitwiseAnd:
            return builder_.emitBinary(OpCode::And, lhs, rhs);
        case ast::AssignmentOperator::AssignBitwiseOr:
            return builder_.emitBinary(OpCode::Or, lhs, rhs);
        case ast::AssignmentOperator::AssignBitwiseXor:
            return builder_.emitBinary(OpCode::Xor, lhs, rhs);
        default:
            throw std::logic_error("single assign is not a compound operation");
    }
}

BodyResult FunctionLowering::lowerIf(const ast::IfExpression& expression) {
    BodyResult condition = lowerExpr(*expression.condition);
    if (!condition.reaches_end) {
        return condition;
    }
    IrBuilder::requireIr(condition.value.has_value() && condition.value->type == module_.types.i1(), "if condition must produce i1");
    semantic::TyId result_type = body_.expressions.at(&expression).type;
    const auto& type = model_.typeContext().get(result_type);
    std::optional<Place> result_slot;
    if (!std::holds_alternative<semantic::UnitTy>(type) && !std::holds_alternative<semantic::NeverTy>(type)) {
        result_slot = createTemporary(result_type);
    }
    BlockId then_block = builder_.createBlock(ir_function_);
    BlockId else_block = builder_.createBlock(ir_function_);
    builder_.emitCondBr(*condition.value, then_block, else_block);
    builder_.clearInsertionPoint();
    std::optional<BlockId> merge;
    auto finishArm = [&](const BodyResult& result) {
        if (!result.reaches_end) {
            return;
        }
        if (result_slot) {
            IrBuilder::requireIr(result.value.has_value(), "if branch has no result value");
            storePlace(*result_slot, *result.value);
        } else {
            IrBuilder::requireIr(!result.value.has_value(), "unit brnach has unexpected runtime value");
        }
        if (!merge) {
            merge = builder_.createBlock(ir_function_);
        }
        builder_.emitBr(*merge);
        builder_.clearInsertionPoint();
    };
    builder_.positionAtEnd(ir_function_, then_block);
    finishArm(lowerExpr(*expression.then_block));
    builder_.positionAtEnd(ir_function_, else_block);
    if (expression.else_branch) {
        finishArm(lowerExpr(*expression.else_branch));
    } else {
        finishArm(BodyResult{true, std::nullopt});
    }
    if (!merge) {
        builder_.clearInsertionPoint();
        return {true, std::nullopt};
    }
    builder_.positionAtEnd(ir_function_, *merge);
    if (result_slot) {
        return {true, loadPlace(*result_slot)};
    }
    return {true, std::nullopt};
}

BodyResult FunctionLowering::lowerShortCircuit(const ast::BinaryExpression& expression) {
    BodyResult left = lowerExpr(*expression.lhs_operand);
    if (!left.reaches_end) {
        return left;
    }
    IrBuilder::requireIr(left.value.has_value() && left.value->type == module_.types.i1(), "logical left operand must produce i1");
    semantic::TyId result_type = body_.expressions.at(&expression).type;
    Place result_slot = createTemporary(result_type);
    storePlace(result_slot, *left.value);
    BlockId rhs_block = builder_.createBlock(ir_function_);
    BlockId merge_block = builder_.createBlock(ir_function_);
    if (expression.op == ast::BinaryOperator::LogicalAnd) {
        builder_.emitCondBr(*left.value, rhs_block, merge_block);
    } else {
        builder_.emitCondBr(*left.value, merge_block, rhs_block);
    }
    builder_.clearInsertionPoint();
    builder_.positionAtEnd(ir_function_, rhs_block);
    BodyResult right = lowerExpr(*expression.rhs_operand);
    if (right.reaches_end) {
        IrBuilder::requireIr(right.value.has_value() && right.value->type == module_.types.i1(), "logical right operand must produce i1");
        storePlace(result_slot, *right.value);
        builder_.emitBr(merge_block);
        builder_.clearInsertionPoint();
    }
    builder_.positionAtEnd(ir_function_, merge_block);
    return {true, loadPlace(result_slot)};
}

} // namespace ir