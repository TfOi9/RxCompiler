#pragma once
#include "antlr4-runtime.h"
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

};

} // namespace ast