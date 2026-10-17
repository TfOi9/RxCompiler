#include "ir/function_lowering.hpp"
#include "ast/ast.hpp"
#include "ir/builder.hpp"
#include "ir/instruction.hpp"
#include "ir/layout.hpp"
#include "ir/module_lowering.hpp"
#include "semantic/body_semantics.hpp"
#include "semantic/integer_literal.hpp"
#include "semantic/path_resolution.hpp"
#include "semantic/semantic_ids.hpp"
#include "semantic/types.hpp"
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
        switch (conversion->kind) {
            case semantic::CoercionKind::Identity:
            case semantic::CoercionKind::NeverToAny:
                break;
            case semantic::CoercionKind::BorrowShared:
            case semantic::CoercionKind::BorrowMutable:
                // TODO unimplemented
                throw std::logic_error("unimplemented!");
            case semantic::CoercionKind::Recovery:
                throw std::logic_error("cannot lower a recovery coercion");
        }
    }
    BodyResult result = lowerExprRaw(expression);
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
    const auto& type = model_.typeContext().get(plan.source_type);
    const auto* primary = std::get_if<semantic::PrimaryTy>(&type);
    IrBuilder::requireIr(
        primary != nullptr &&
        plan.source_type == plan.operation_type &&
        plan.reference_depth == 0 &&
        !plan.outer_mutable_to_shared &&
        (plan.read_kind == semantic::OperandReadKind::Value || plan.read_kind == semantic::OperandReadKind::EqualityBorrow),
        "non-primitive operand lowering is not implemented"
    );
    return primary->ty;
}

BodyResult FunctionLowering::lowerUnary(const ast::UnaryExpression& expression) {
    if (expression.op != ast::UnaryOperator::Negation || expression.op != ast::UnaryOperator::Not) {
        // TODO unimplemented
        throw std::logic_error("borrow or dereference is not implemented");
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
    Operand value = *result.value;
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
        // TODO unimplemented
        throw std::logic_error("short-circuit lowering is not implemented");
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
    Operand lhs = *left.value;
    Operand rhs = *right.value;
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

} // namespace ir