#pragma once
#include "ast.hpp"
#include <iostream>
#include <ostream>

namespace ast {

void dumpAst(const Crate& root, std::ostream& os);

void printIndent(int depth, std::ostream& os);

void dumpItem(const AstPtr<Item>& ast, std::ostream& os, int depth);

void dumpFunction(const FunctionItem& ast, std::ostream& os, int depth);
void dumpStruct(const StructItem& ast, std::ostream& os, int depth);
void dumpConstant(const ConstantItem& ast, std::ostream& os, int depth);
void dumpImpl(const ImplItem& ast, std::ostream& os, int depth);

void dumpGenericParams(const std::vector<GenericParam>& ast, std::ostream& os, int depth);
void dumpGenericParam(const GenericParam& ast, std::ostream& os, int depth);

void dumpLifetime(const Lifetime& ast, std::ostream& os, int depth);
void dumpLifetimeBounds(const LifetimeBounds& ast, std::ostream& os, int depth);

void dumpSelfParam(const SelfParam& ast, std::ostream& os, int depth);
void dumpFunctionParams(const std::vector<FunctionParam>& ast, std::ostream& os, int depth);
void dumpFunctionParam(const FunctionParam& ast, std::ostream& os, int depth);

void dumpTypeRef(const AstPtr<TypeRef>& ast, std::ostream& os, int depth);
void dumpUnitType(const UnitType* ast, std::ostream& os, int depth);
void dumpTypePath(const TypePath* ast, std::ostream& os, int depth);
void dumpReferenceType(const ReferenceType* ast, std::ostream& os, int depth);
void dumpArrayType(const ArrayType* ast, std::ostream& os, int depth);
void dumpParenthesizedType(const ParenthesizedType* ast, std::ostream& os, int depth);
void dumpTypePathSegment(const TypePathSegment& ast, std::ostream& os, int depth);
void dumpPathIdentSegment(const PathIdentSegment& ast, std::ostream& os, int depth);
void dumpGenericArgs(const GenericArgs& ast, std::ostream& os, int depth);
void dumpGenericArg(const GenericArg& ast, std::ostream& os, int depth);

void dumpWhereClause(const WhereClause& ast, std::ostream& os, int depth);
void dumpLifetimeWhereClauseItem(const LifetimeWhereClauseItem& ast, std::ostream& os, int depth);
void dumpTypeBoundWhereClauseItem(const TypeBoundWhereClauseItem& ast, std::ostream& os, int depth);
void dumpTypeParamBounds(const TypeParamBounds& ast, std::ostream& os, int depth);

void dumpExpression(const AstPtr<Expression>& ast, std::ostream& os, int depth);
void dumpBlockExpression(const BlockExpression& ast, std::ostream& os, int depth);
void dumpIntegerExpression(const IntegerExpression* ast, std::ostream& os, int depth);
void dumpBoolExpression(const BoolExpression* ast, std::ostream& os, int depth);
void dumpUnitExpression(const UnitExpression* ast, std::ostream& os, int depth);
void dumpUnaryExpression(const UnaryExpression* ast, std::ostream& os, int depth);
void dumpBinaryExpression(const BinaryExpression* ast, std::ostream& os, int depth);
void dumpAssignmentExpression(const AssignmentExpression* ast, std::ostream& os, int depth);
void dumpPathExpression(const PathExpression* ast, std::ostream& os, int depth);
void dumpCallExpression(const CallExpression* ast, std::ostream& os, int depth);
void dumpArrayExpression(const ArrayExpression* ast, std::ostream& os, int depth);
void dumpIndexExpression(const IndexExpression* ast, std::ostream& os, int depth);
void dumpFieldExpression(const FieldExpression* ast, std::ostream& os, int depth);
void dumpMethodCallExpression(const MethodCallExpression* ast, std::ostream& os, int depth);
void dumpCastExpression(const CastExpression* ast, std::ostream& os, int depth);
void dumpStructExpression(const StructExpression* ast, std::ostream& os, int depth);
void dumpStructExprField(const StructExprField& ast, std::ostream& os, int depth);
void dumpIfExpression(const IfExpression* ast, std::ostream& os, int depth);
void dumpLoopExpression(const LoopExpression* ast, std::ostream& os, int depth);
void dumpWhileExpression(const WhileExpression* ast, std::ostream& os, int depth);
void dumpBreakExpression(const BreakExpression* ast, std::ostream& os, int depth);
void dumpContinueExpression(const ContinueExpression* ast, std::ostream& os, int depth);
void dumpReturnExpression(const ReturnExpression* ast, std::ostream& os, int depth);
void dumpGroupedExpression(const GroupedExpression* ast, std::ostream& os, int depth);

void dumpStatement(const AstPtr<Statement>& ast, std::ostream& os, int depth);
void dumpLetStatement(const LetStatement* ast, std::ostream& os, int depth);
void dumpExpressionStatement(const ExpressionStatement* ast, std::ostream& os, int depth);
void dumpIdentifierBinding(const IdentifierBinding& ast, std::ostream& os, int depth);

void dumpConstValue(const ConstValue& ast, std::ostream& os, int depth);
void dumpIntegerLiteralValue(const IntegerLiteralValue& ast, std::ostream& os, int depth);
void dumpPathInExpression(const PathInExpression& ast, std::ostream& os, int depth);
void dumpPathExprSegment(const PathExprSegment& ast, std::ostream& os, int depth);
void dumpMagnitude(const Magnitude& ast, std::ostream& os, int depth);

void dumpOuterAttributes(const OuterAttribute& ast, std::ostream& os, int depth);
void dumpStructField(const StructField& ast, std::ostream& os, int depth);
void dumpAssociatedItems(const std::vector<AssociatedItem>& ast, std::ostream& os, int depth);
void dumpAssociatedItem(const AssociatedItem& ast, std::ostream& os, int depth);

void dumpUnaryOperator(const UnaryOperator& op, std::ostream& os);
void dumpBinaryOperator(const BinaryOperator& op, std::ostream& os);
void dumpAssignmentOperator(const AssignmentOperator& op, std::ostream& os);

} // namespace ast