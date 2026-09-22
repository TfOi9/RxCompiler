#include "builder.hpp"
#include "ast.hpp"

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
            beginToken->getTokenIndex(),
            beginToken->getLine(),
            beginToken->getCharPositionInLine()
        },
        SourceLocation {
            endToken->getTokenIndex() + 1,
            endToken->getLine(),
            endToken->getCharPositionInLine() + endToken->getText().size()
        }
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

} // namespace ast