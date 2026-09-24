#pragma once
#include "../generated/RxParser.h"
#include "ast.hpp"
#include <string_view>

namespace ast {

class AstBuilder {
public:
    explicit AstBuilder(std::string_view source): source_(source) {};
    AstPtr<Crate> build(RxParser::CrateContext* ctx);

private:
    std::string_view source_;

    SourceSpan makeSpan(antlr4::ParserRuleContext* ctx) const;
    std::string getText(antlr4::tree::TerminalNode* node) const;
    std::string buildIdentifier(RxParser::IdentifierContext* ctx);
    Lifetime buildLifetime(RxParser::LifetimeContext* ctx);
    LifetimeBounds buildLifetimeBounds(RxParser::LifetimeBoundsContext* ctx);
    IdentifierBinding buildIdentifierBinding(RxParser::IdentifierBindingContext* ctx);
    
    AstPtr<Item> buildItem(RxParser::ItemContext* ctx);
    AstPtr<FunctionItem> buildFunctionItem(RxParser::FunctionDefinitionContext* ctx);
    AstPtr<StructItem> buildStructItem(RxParser::StructDefinitionContext* ctx);
    AstPtr<ConstantItem> buildConstantItem(RxParser::ConstantItemContext* ctx);
    AstPtr<ImplItem> buildImplItem(RxParser::InherentImplContext* ctx);

    std::vector<GenericParam> buildGenericParams(RxParser::GenericParamsContext* ctx);
    GenericParam buildGenericParam(RxParser::LifetimeParamContext* ctx);
    SelfParam buildSelfParam(RxParser::SelfParamContext* ctx);
    std::vector<FunctionParam> buildFunctionParams(RxParser::FunctionParametersContext* ctx);
    FunctionParam buildFunctionParam(RxParser::FunctionParamContext* ctx);
    WhereClause buildWhereClause(RxParser::WhereClauseContext* ctx);
    WhereClauseItem buildWhereClauseItem(RxParser::WhereClauseItemContext* ctx);

    std::vector<OuterAttribute> buildOuterAttributes(const std::vector<RxParser::OuterAttributeContext*>& ctx);
    OuterAttribute buildDeriveAttribute(RxParser::OuterAttributeContext* ctx);
    std::vector<StructField> buildStructFields(const std::vector<RxParser::StructFieldContext*>& ctx);

    AstPtr<Statement> buildStatement(RxParser::StatementContext* ctx);
    
    AstPtr<Expression> buildExpression(RxParser::ExpressionContext* ctx);
    AstPtr<BlockExpression> buildBlockExpression(RxParser::BlockExpressionContext* ctx);
    AstPtr<Expression> buildConditionExpression(RxParser::ConditionExpressionContext* ctx);

    AstPtr<TypeRef> buildTypeRef(RxParser::TypeRefContext* ctx);
    TypeParamBounds buildTypeParamBounds(RxParser::TypeParamBoundsContext* ctx);
    AstPtr<ConstValue> buildConstValue(RxParser::ConstValueContext* ctx);

    IntegerSuffix getSuffix(const std::string& spelling);

    AstPtr<Magnitude> buildMagnitude(RxParser::MagnitudeContext* ctx);
    
    PathInExpression buildPathInExpression(RxParser::PathInExpressionContext* ctx);
    PathExprSegment buildPathExprSegment(RxParser::PathExprSegmentContext* ctx);
    PathIdentSegment buildPathIdentSegment(RxParser::PathIdentSegmentContext* ctx);
    GenericArgs buildGenericArgs(RxParser::GenericArgsContext* ctx);
    GenericArg buildGenericArg(RxParser::GenericArgContext* ctx);

    std::vector<AssociatedItem> buildAssociatedItems(const std::vector<RxParser::AssociatedItemContext*>& ctx);
    AssociatedItem buildAssociatedItem(RxParser::AssociatedItemContext* ctx);

    AstPtr<TypePath> buildTypePath(RxParser::TypePathContext* ctx);
    TypePathSegment buildTypePathSegment(RxParser::TypePathSegmentContext* ctx);
    AstPtr<ReferenceType> buildReferenceType(RxParser::ReferenceTypeContext* ctx);
    AstPtr<ArrayType> buildArrayType(RxParser::ArrayTypeContext* ctx);

    AstPtr<LetStatement> buildLetStatement(RxParser::LetStatementContext* ctx);
    AstPtr<Expression> buildExpressionWithBlock(RxParser::ExpressionWithBlockContext* ctx);
    AstPtr<Expression> buildStatementExpression(RxParser::StatementExpressionContext* ctx);
    AstPtr<IfExpression> buildIfExpression(RxParser::IfExpressionContext* ctx);
    AstPtr<LoopExpression> buildLoopExpression(RxParser::ExpressionWithBlockContext* ctx);
    AstPtr<WhileExpression> buildWhileExpression(RxParser::ExpressionWithBlockContext* ctx);

    AstPtr<Expression> buildAssignmentExpression(RxParser::AssignmentExpressionContext* ctx);
    AstPtr<Expression> buildLogicalOrExpression(RxParser::LogicalOrExpressionContext* ctx);
    AstPtr<Expression> buildLogicalAndExpression(RxParser::LogicalAndExpressionContext* ctx);
    AstPtr<Expression> buildComparisonExpression(RxParser::ComparisonExpressionContext* ctx);
    BinaryOperator buildComparisonExceptLtContext(RxParser::ComparisonExceptLtContext* ctx);
    AstPtr<Expression> buildClosedBitOrExpression(RxParser::ClosedBitOrExpressionContext* ctx);
    AstPtr<Expression> buildBitOrExpression(RxParser::BitOrExpressionContext* ctx);
    AstPtr<Expression> buildClosedBitXorExpression(RxParser::ClosedBitXorExpressionContext* ctx);
    AstPtr<Expression> buildBitXorExpression(RxParser::BitXorExpressionContext* ctx);
    AstPtr<Expression> buildClosedBitAndExpression(RxParser::ClosedBitAndExpressionContext* ctx);
    AstPtr<Expression> buildBitAndExpression(RxParser::BitAndExpressionContext* ctx);
    AstPtr<Expression> buildClosedShiftExpression(RxParser::ClosedShiftExpressionContext* ctx);
    AstPtr<Expression> buildShiftExpression(RxParser::ShiftExpressionContext* ctx);
    AstPtr<Expression> buildClosedAdditiveExpression(RxParser::ClosedAdditiveExpressionContext* ctx);
    AstPtr<Expression> buildAdditiveExpression(RxParser::AdditiveExpressionContext* ctx);
    BinaryOperator buildAdditiveOperator(RxParser::AdditiveOperatorContext* ctx);
    AstPtr<Expression> buildClosedMultiplicativeExpression(RxParser::ClosedMultiplicativeExpressionContext* ctx);
    AstPtr<Expression> buildMultiplicativeExpression(RxParser::MultiplicativeExpressionContext* ctx);
    BinaryOperator buildMultiplicativeOperator(RxParser::MultiplicativeOperatorContext* ctx);
    AstPtr<Expression> buildClosedCastExpression(RxParser::ClosedCastExpressionContext* ctx);
    AstPtr<Expression> buildCastExpression(RxParser::CastExpressionContext* ctx);
    AstPtr<TypeRef> buildClosedCastType(RxParser::ClosedCastTypeContext* ctx);
    AstPtr<Expression> buildUnaryExpression(RxParser::UnaryExpressionContext* ctx);
    UnaryOperator buildUnaryOperator(RxParser::UnaryOperatorContext* ctx);
    AstPtr<Expression> buildPostfixExpression(RxParser::PostfixExpressionContext* ctx);
    AstPtr<Expression> buildPrimaryExpression(RxParser::PrimaryExpressionContext* ctx);

    AstPtr<Expression> buildLiteralExpression(RxParser::LiteralExpressionContext* ctx);
    AstPtr<Expression> buildPathOrStructExpression(RxParser::NonBlockPrimaryContext* ctx);
    AstPtr<Expression> buildArrayExpression(RxParser::ArrayExpressionContext* ctx);

    AstPtr<Expression> buildConditionAssignmentExpression(RxParser::ConditionAssignmentExpressionContext* ctx);
    AstPtr<Expression> buildConditionLogicalOrExpression(RxParser::ConditionLogicalOrExpressionContext* ctx);
    AstPtr<Expression> buildConditionLogicalAndExpression(RxParser::ConditionLogicalAndExpressionContext* ctx);
    AstPtr<Expression> buildConditionComparisonExpression(RxParser::ConditionComparisonExpressionContext* ctx);
    AstPtr<Expression> buildConditionClosedBitOrExpression(RxParser::ConditionClosedBitOrExpressionContext* ctx);
    AstPtr<Expression> buildConditionBitOrExpression(RxParser::ConditionBitOrExpressionContext* ctx);
    AstPtr<Expression> buildConditionClosedBitXorExpression(RxParser::ConditionClosedBitXorExpressionContext* ctx);
    AstPtr<Expression> buildConditionBitXorExpression(RxParser::ConditionBitXorExpressionContext* ctx);
    AstPtr<Expression> buildConditionClosedBitAndExpression(RxParser::ConditionClosedBitAndExpressionContext* ctx);
    AstPtr<Expression> buildConditionBitAndExpression(RxParser::ConditionBitAndExpressionContext* ctx);
    AstPtr<Expression> buildConditionClosedShiftExpression(RxParser::ConditionClosedShiftExpressionContext* ctx);
    AstPtr<Expression> buildConditionShiftExpression(RxParser::ConditionShiftExpressionContext* ctx);
    AstPtr<Expression> buildConditionClosedAdditiveExpression(RxParser::ConditionClosedAdditiveExpressionContext* ctx);
    AstPtr<Expression> buildConditionAdditiveExpression(RxParser::ConditionAdditiveExpressionContext* ctx);
    AstPtr<Expression> buildConditionClosedMultiplicativeExpression(RxParser::ConditionClosedMultiplicativeExpressionContext* ctx);
    AstPtr<Expression> buildConditionMultiplicativeExpression(RxParser::ConditionMultiplicativeExpressionContext* ctx);
    AstPtr<Expression> buildConditionClosedCastExpression(RxParser::ConditionClosedCastExpressionContext* ctx);
    AstPtr<Expression> buildConditionCastExpression(RxParser::ConditionCastExpressionContext* ctx);
    AstPtr<Expression> buildConditionUnaryExpression(RxParser::ConditionUnaryExpressionContext* ctx);
    AstPtr<Expression> buildConditionPostfixExpression(RxParser::ConditionPostfixExpressionContext* ctx);
    AstPtr<Expression> buildConditionPrimary(RxParser::ConditionPrimaryContext* ctx);
    AstPtr<Expression> buildConditionPrimaryWithoutBareBlock(RxParser::ConditionPrimaryWithoutBareBlockContext* ctx);

    AstPtr<Expression> buildConditionBreakExpression(RxParser::ConditionBreakExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakAssignmentExpression(RxParser::ConditionBreakAssignmentExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakLogicalOrExpression(RxParser::ConditionBreakLogicalOrExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakLogicalAndExpression(RxParser::ConditionBreakLogicalAndExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakComparisonExpression(RxParser::ConditionBreakComparisonExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakClosedBitOrExpression(RxParser::ConditionBreakClosedBitOrExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakBitOrExpression(RxParser::ConditionBreakBitOrExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakClosedBitXorExpression(RxParser::ConditionBreakClosedBitXorExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakBitXorExpression(RxParser::ConditionBreakBitXorExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakClosedBitAndExpression(RxParser::ConditionBreakClosedBitAndExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakBitAndExpression(RxParser::ConditionBreakBitAndExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakClosedShiftExpression(RxParser::ConditionBreakClosedShiftExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakShiftExpression(RxParser::ConditionBreakShiftExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakClosedAdditiveExpression(RxParser::ConditionBreakClosedAdditiveExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakAdditiveExpression(RxParser::ConditionBreakAdditiveExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakClosedMultiplicativeExpression(RxParser::ConditionBreakClosedMultiplicativeExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakMultiplicativeExpression(RxParser::ConditionBreakMultiplicativeExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakClosedCastExpression(RxParser::ConditionBreakClosedCastExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakCastExpression(RxParser::ConditionBreakCastExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakUnaryExpression(RxParser::ConditionBreakUnaryExpressionContext* ctx);
    AstPtr<Expression> buildConditionBreakPostfixExpression(RxParser::ConditionBreakPostfixExpressionContext* ctx);

    AstPtr<Expression> buildStatementAssignmentExpression(RxParser::StatementAssignmentExpressionContext* ctx);
    AstPtr<Expression> buildStatementLogicalOrExpression(RxParser::StatementLogicalOrExpressionContext* ctx);
    AstPtr<Expression> buildStatementLogicalAndExpression(RxParser::StatementLogicalAndExpressionContext* ctx);
    AstPtr<Expression> buildStatementComparisonExpression(RxParser::StatementComparisonExpressionContext* ctx);
    AstPtr<Expression> buildStatementClosedBitOrExpression(RxParser::StatementClosedBitOrExpressionContext* ctx);
    AstPtr<Expression> buildStatementBitOrExpression(RxParser::StatementBitOrExpressionContext* ctx);
    AstPtr<Expression> buildStatementClosedBitXorExpression(RxParser::StatementClosedBitXorExpressionContext* ctx);
    AstPtr<Expression> buildStatementBitXorExpression(RxParser::StatementBitXorExpressionContext* ctx);
    AstPtr<Expression> buildStatementClosedBitAndExpression(RxParser::StatementClosedBitAndExpressionContext* ctx);
    AstPtr<Expression> buildStatementBitAndExpression(RxParser::StatementBitAndExpressionContext* ctx);
    AstPtr<Expression> buildStatementClosedShiftExpression(RxParser::StatementClosedShiftExpressionContext* ctx);
    AstPtr<Expression> buildStatementShiftExpression(RxParser::StatementShiftExpressionContext* ctx);
    AstPtr<Expression> buildStatementClosedAdditiveExpression(RxParser::StatementClosedAdditiveExpressionContext* ctx);
    AstPtr<Expression> buildStatementAdditiveExpression(RxParser::StatementAdditiveExpressionContext* ctx);
    AstPtr<Expression> buildStatementClosedMultiplicativeExpression(RxParser::StatementClosedMultiplicativeExpressionContext* ctx);
    AstPtr<Expression> buildStatementMultiplicativeExpression(RxParser::StatementMultiplicativeExpressionContext* ctx);
    AstPtr<Expression> buildStatementClosedCastExpression(RxParser::StatementClosedCastExpressionContext* ctx);
    AstPtr<Expression> buildStatementCastExpression(RxParser::StatementCastExpressionContext* ctx);
    AstPtr<Expression> buildStatementUnaryExpression(RxParser::StatementUnaryExpressionContext* ctx);
    AstPtr<Expression> buildStatementPostfixExpression(RxParser::StatementPostfixExpressionContext* ctx);

};

} // namespace ast
