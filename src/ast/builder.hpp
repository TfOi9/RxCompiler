#pragma once
#include "antlr4-runtime.h"
#include "../generated/RxParser.h"
#include "ast.hpp"
#include <memory>
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

    AstPtr<Statement> buildStatement(RxParser::StatementContext* ctx);
    AstPtr<ExpressionStatement> buildExpressionStatement(RxParser::StatementExpressionContext* ctx);

    AstPtr<Expression> buildExpression(RxParser::ExpressionContext* ctx);
    AstPtr<BlockExpression> buildBlockExpression(RxParser::BlockExpressionContext* ctx);
    AstPtr<Expression> buildConditionExpression(RxParser::ConditionExpressionContext* ctx);

    AstPtr<TypeRef> buildTypeRef(RxParser::TypeRefContext* ctx);
    TypeParamBounds buildTypeParamBounds(RxParser::TypeParamBoundsContext* ctx);
    AstPtr<ConstValue> buildConstValue(RxParser::ConstValueContext* ctx);
    
    PathInExpression buildPathInExpression(RxParser::PathInExpressionContext* ctx);
    PathExprSegment buildPathExprSegment(RxParser::PathExprSegmentContext* ctx);
    PathIdentSegment buildPathIdentSegment(RxParser::PathIdentSegmentContext* ctx);

};

} // namespace ast