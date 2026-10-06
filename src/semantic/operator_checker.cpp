#include "operator_checker.hpp"
#include "ast/ast.hpp"
#include "semantic/body_semantics.hpp"
#include "semantic/integer_literal.hpp"
#include "semantic/struct_resolver.hpp"
#include "semantic/types.hpp"
#include <optional>
#include <variant>

namespace semantic {

std::optional<PrimaryTyKind> OperatorChecker::primaryKind(const TypeContext& types, TyId type) {
    if (const auto* p = std::get_if<PrimaryTy>(&types.get(type))) {
        return p->ty;
    }
    return std::nullopt;
}

bool OperatorChecker::integerType(const TypeContext& types, TyId type) {
    auto kind = primaryKind(types, type);
    return kind && isIntegerKind(*kind);
}

bool OperatorChecker::signedIntegerType(const TypeContext& types, TyId type) {
    auto kind = primaryKind(types, type);
    return kind && isSignedKind(*kind);
}

std::optional<OperandPlan> OperatorChecker::scalarOperand(TyId source) const {
    const auto& info = types_.get(source);
    if (std::holds_alternative<PrimaryTy>(info)) {
        return OperandPlan{
            source, source, OperandReadKind::Value, false, 0
        };
    }
    const auto* ref = std::get_if<RefTy>(&info);
    if (!ref || ref->is_mut) {
        return std::nullopt;
    }
    if (!std::holds_alternative<PrimaryTy>(types_.get(ref->target))) {
        return std::nullopt;
    }
    return OperandPlan{
        source, ref->target, OperandReadKind::SharedReferent, false, 1
    };
}

std::optional<UnaryPlan> OperatorChecker::checkScalarUnary(ast::UnaryOperator op, TyId operand) {
    auto p = scalarOperand(operand);
    if (!p) {
        return std::nullopt;
    }
    if (op == ast::UnaryOperator::Negation) {
        if (!signedIntegerType(types_, p->operation_type)) {
            return std::nullopt;
        }
    } else if (op != ast::UnaryOperator::Not) {
        return std::nullopt;
    }
    return UnaryPlan{*p, p->operation_type};
}

std::optional<BinaryPlan> OperatorChecker::checkArithmeticOperands(TyId left, TyId right) {
    auto l = scalarOperand(left);
    auto r = scalarOperand(right);
    if (!l || !r || l->operation_type != r->operation_type || !integerType(types_, l->operation_type)) {
        return std::nullopt;
    }
    return BinaryPlan{*l, *r, l->operation_type};
}

std::optional<BinaryPlan> OperatorChecker::checkBitwiseOperands(TyId left, TyId right) {
    auto l = scalarOperand(left);
    auto r = scalarOperand(right);
    if (!l || !r || l->operation_type != r->operation_type) {
        return std::nullopt;
    }
    return BinaryPlan{*l, *r, l->operation_type};
}

std::optional<BinaryPlan> OperatorChecker::checkShiftOperands(TyId left, TyId right) {
    auto l = scalarOperand(left);
    auto r = scalarOperand(right);
    if (!l || !r || !integerType(types_, l->operation_type) || !integerType(types_, r->operation_type)) {
        return std::nullopt;
    }
    return BinaryPlan{*l, *r, l->operation_type};
}

std::optional<BinaryPlan> OperatorChecker::checkEqualityOperands(TyId left, TyId right) {
    if (left != right || !derives_.supports(left, DeriveKind::PartialEq)) {
        return std::nullopt;
    }
    OperandPlan l{left, left, OperandReadKind::EqualityBorrow};
    OperandPlan r{right, right, OperandReadKind::EqualityBorrow};
    return BinaryPlan{l, r, bool_type_};
}

std::optional<BinaryPlan> OperatorChecker::checkOrderingOperands(TyId left, TyId right) {
    bool adjust_right = false;
    if (left != right) {
        const auto* l = std::get_if<RefTy>(&types_.get(left));
        const auto* r = std::get_if<RefTy>(&types_.get(right));
        if (!l || !r || l->is_mut || !r->is_mut || l->target != r->target) {
            return std::nullopt;
        }
        adjust_right = true;
    }
    TyId scalar = left;
    size_t depth = 0;
    while (const auto* ref = std::get_if<RefTy>(&types_.get(scalar))) {
        scalar = ref->target;
        depth++;
    }
    if (!std::holds_alternative<PrimaryTy>(types_.get(scalar))) {
        return std::nullopt;
    }
    const auto read = depth == 0 ? OperandReadKind::Value : OperandReadKind::OrderedReferent;
    OperandPlan l{left, scalar, read, false, depth};
    OperandPlan r{right, scalar, read, adjust_right, depth};
    return BinaryPlan{l, r, bool_type_};
}

} // namespace semantic