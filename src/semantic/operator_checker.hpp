#pragma once
#include "ast/ast.hpp"
#include "semantic/body_semantics.hpp"
#include "semantic/derive_checker.hpp"
#include "semantic/types.hpp"
#include <optional>

namespace semantic {

class OperatorChecker {
public:
    OperatorChecker(TypeContext& types, DeriveChecker& derives): types_(types), derives_(derives), bool_type_(types.insert(PrimaryTy{PrimaryTyKind::Bool})) {}
    std::optional<OperandPlan> scalarOperand(TyId source) const;
    std::optional<UnaryPlan> checkScalarUnary(ast::UnaryOperator op, TyId operand);
    std::optional<BinaryPlan> checkArithmeticOperands(TyId left, TyId right);
    std::optional<BinaryPlan> checkBitwiseOperands(TyId left, TyId right);
    std::optional<BinaryPlan> checkShiftOperands(TyId left, TyId right);
    std::optional<BinaryPlan> checkEqualityOperands(TyId left, TyId right);
    std::optional<BinaryPlan> checkOrderingOperands(TyId left, TyId right);
private:
    TypeContext& types_;
    DeriveChecker& derives_;
    TyId bool_type_;

    static std::optional<PrimaryTyKind> primaryKind(const TypeContext& types, TyId type);
    static bool integerType(const TypeContext& types, TyId type);
    static bool signedIntegerType(const TypeContext& types, TyId type);
};

} // namespace semantic