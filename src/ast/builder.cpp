#include "builder.hpp"
#include "ast.hpp"
#include <optional>

namespace ast {

AstPtr<Crate> AstBuilder::build(RxParser::CrateContext* ctx) {
    auto crate = std::make_unique<Crate>(makeSpan(ctx));
    for (auto* itemContext: ctx->item()) {
        auto item = buildItem(itemContext);
        if (item != nullptr) {
            crate->items.push_back(std::move(item));
        }
    }
    return crate;
}

SourceSpan AstBuilder::makeSpan(antlr4::ParserRuleContext* ctx) const {
    auto* beginToken = ctx->start;
    auto* endToken = ctx->stop;
    return SourceSpan {
        SourceLocation {
            beginToken->getStartIndex(),
            beginToken->getLine(),
            beginToken->getCharPositionInLine()
        },
        SourceLocation {
            endToken->getStopIndex() + 1,
            endToken->getLine(),
            endToken->getCharPositionInLine() + endToken->getText().size()
        }
    };
}

std::string AstBuilder::getText(antlr4::tree::TerminalNode* node) const {
    return node->getText();
}

std::string AstBuilder::buildIdentifier(RxParser::IdentifierContext* ctx) {
    return ctx->toString();
}

Lifetime AstBuilder::buildLifetime(RxParser::LifetimeContext* ctx) {
    return Lifetime {
        makeSpan(ctx),
        ctx->LIFETIME()->getText()
    };
}

LifetimeBounds AstBuilder::buildLifetimeBounds(RxParser::LifetimeBoundsContext* ctx) {
    std::vector<Lifetime> lifetimes;
    for (auto* lt: ctx->lifetime()) {
        lifetimes.push_back(buildLifetime(lt));
    }
    return LifetimeBounds {
        makeSpan(ctx),
        lifetimes
    };
}

IdentifierBinding AstBuilder::buildIdentifierBinding(RxParser::IdentifierBindingContext* ctx) {
    return IdentifierBinding {
        makeSpan(ctx),
        ctx->MUT() == nullptr,
        ctx->identifier()->getText()
    };
}

AstPtr<Item> AstBuilder::buildItem(RxParser::ItemContext* ctx) {
    if (ctx->functionDefinition()) {
        return buildFunctionItem(ctx->functionDefinition());
    }
    if (ctx->structDefinition()) {
        return buildStructItem(ctx->structDefinition());
    }
    if (ctx->constantItem()) {
        return buildConstantItem(ctx->constantItem());
    }
    if (ctx->inherentImpl()) {
        return buildImplItem(ctx->inherentImpl());
    }
    if (ctx->useDeclaration()) {
        // future plugins here
        return nullptr;
    }
    return nullptr;
}

std::vector<GenericParam> AstBuilder::buildGenericParams(RxParser::GenericParamsContext* ctx) {
    std::vector<GenericParam> params;
    for (auto* param: ctx->lifetimeParam()) {
        params.push_back(buildGenericParam(param));
    }
    return params;
}

GenericParam AstBuilder::buildGenericParam(RxParser::LifetimeParamContext* ctx) {
    std::optional<LifetimeBounds> bounds = std::nullopt;
    if (ctx->lifetimeBounds()) {
        bounds = buildLifetimeBounds(ctx->lifetimeBounds());
    }
    return GenericParam {
        makeSpan(ctx),
        buildLifetime(ctx->lifetime()),
        bounds
    };
}

SelfParam AstBuilder::buildSelfParam(RxParser::SelfParamContext* ctx) {
    return SelfParam {
        makeSpan(ctx),
        ctx->AMP() != nullptr,
        ctx->MUT() != nullptr,
        ctx->lifetime() ? std::optional<Lifetime>(buildLifetime(ctx->lifetime())) : std::nullopt
    };
}

std::vector<FunctionParam> AstBuilder::buildFunctionParams(RxParser::FunctionParametersContext* ctx) {
    std::vector<FunctionParam> params;
    for (auto* param: ctx->functionParam()) {
        params.push_back(buildFunctionParam(param));
    }
    return params;
}

FunctionParam AstBuilder::buildFunctionParam(RxParser::FunctionParamContext* ctx) {
    return FunctionParam {
        makeSpan(ctx),
        buildIdentifier(ctx->identifierBinding()->identifier()),
        ctx->identifierBinding()->MUT() != nullptr,
        buildTypeRef(ctx->typeRef())
    };
}

WhereClause AstBuilder::buildWhereClause(RxParser::WhereClauseContext* ctx) {
    WhereClause clause;
    clause.span = makeSpan(ctx);
    for (auto* item: ctx->whereClauseItem()) {
        clause.items.push_back(buildWhereClauseItem(item));
    }
    return clause;
}

WhereClauseItem AstBuilder::buildWhereClauseItem(RxParser::WhereClauseItemContext* ctx) {
    WhereClauseItem item;
    item.span = makeSpan(ctx);
    if (ctx->lifetime() != nullptr) {
        item.lifetime = LifetimeWhereClauseItem {
            makeSpan(ctx),
            buildLifetime(ctx->lifetime()),
            buildLifetimeBounds(ctx->lifetimeBounds())
        };
    } else if (ctx->typeRef() != nullptr) {
        item.type_bound = TypeBoundWhereClauseItem {
            makeSpan(ctx),
            buildTypeRef(ctx->typeRef()),
            buildTypeParamBounds(ctx->typeParamBounds())
        };
    }
    return item;
}

AstPtr<FunctionItem> AstBuilder::buildFunctionItem(RxParser::FunctionDefinitionContext* ctx) {
    FunctionItem fn(makeSpan(ctx));
    fn.name = buildIdentifier(ctx->identifier());
    fn.body = buildBlockExpression(ctx->blockExpression());
    fn.generic_params = ctx->genericParams() ? buildGenericParams(ctx->genericParams()) : std::vector<GenericParam>();
    fn.self_param = ctx->functionParameters() ? std::optional<SelfParam>(buildSelfParam(ctx->functionParameters()->selfParam())) : std::nullopt;
    fn.function_params = ctx->functionParameters() ? buildFunctionParams(ctx->functionParameters()) : std::vector<FunctionParam>();
    fn.where_clause = ctx->whereClause() ? std::optional<WhereClause>(buildWhereClause(ctx->whereClause())) : std::nullopt;
    fn.return_type = ctx->typeRef() ? buildTypeRef(ctx->typeRef()) : nullptr;
    return std::make_unique<FunctionItem>(std::move(fn));
}

} // namespace ast