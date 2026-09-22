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
    
    AstPtr<Item> buildItem(RxParser::ItemContext* ctx);
    AstPtr<FunctionItem> buildFunctionItem(RxParser::FunctionDefinitionContext* ctx);
    AstPtr<StructItem> buildStructItem(RxParser::StructDefinitionContext* ctx);
    AstPtr<ConstantItem> buildConstantItem(RxParser::ConstantItemContext* ctx);
    AstPtr<ImplItem> buildImplItem(RxParser::InherentImplContext* ctx);

    AstPtr<Statement> buildStatement(RxParser::StatementContext* ctx);
    AstPtr<ExpressionStatement> buildExpressionStatement(RxParser::StatementExpressionContext* ctx);

    AstPtr<Expression> buildExpression(RxParser::ExpressionContext* ctx);
    AstPtr<BlockExpression> buildBlockExpression(RxParser::BlockExpressionContext* ctx);
    AstPtr<Expression> buildConditionExpression(RxParser::ConditionExpressionContext* ctx);

    AstPtr<TypeRef> buildTypeRef(RxParser::TypeRefContext* ctx);
    AstPtr<ConstValue> buildConstValue(RxParser::ConstValueContext* ctx);
    
    PathInExpression buildPathInExpression(RxParser::PathInExpressionContext* ctx);
    PathExprSegment buildPathExprSegment(RxParser::PathExprSegmentContext* ctx);
    PathIdentSegment buildPathIdentSegment(RxParser::PathIdentSegmentContext* ctx);

};

} // namespace ast