#include "builder.hpp"
#include "ast.hpp"
#include <memory>
#include <optional>
#include <vector>

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

std::vector<OuterAttribute> AstBuilder::buildOuterAttributes(const std::vector<RxParser::OuterAttributeContext*>& ctx) {
    std::vector<OuterAttribute> oa;
    for (auto* attr: ctx) {
        oa.push_back(buildDeriveAttribute(attr));
    }
    return oa;
}

OuterAttribute AstBuilder::buildDeriveAttribute(RxParser::OuterAttributeContext* ctx) {
    std::vector<DeriveName> names;
    for (auto* name: ctx->deriveName()) {
        DeriveName type;
        if (name->COPY()) {
            type = DeriveName::Copy;
        } else if (name->CLONE()) {
            type = DeriveName::Clone;
        } else if (name->PARTIAL_EQ()) {
            type = DeriveName::PartialEq;
        } else {
            type = DeriveName::Eq;
        }
        names.push_back(type);
    }
    return OuterAttribute {
        makeSpan(ctx),
        names
    };
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

std::vector<StructField> AstBuilder::buildStructFields(const std::vector<RxParser::StructFieldContext*>& ctx) {
    std::vector<StructField> sfs;
    for (auto* sf: ctx) {
        sfs.push_back(StructField {
            makeSpan(sf),
            buildIdentifier(sf->identifier()),
            buildTypeRef(sf->typeRef())
        });
    }
    return sfs;
}

AstPtr<StructItem> AstBuilder::buildStructItem(RxParser::StructDefinitionContext* ctx) {
    StructItem st(makeSpan(ctx));
    st.name = buildIdentifier(ctx->identifier());
    st.attributes = buildOuterAttributes(ctx->outerAttribute());
    st.generic_params = ctx->genericParams() ? buildGenericParams(ctx->genericParams()) : std::vector<GenericParam>();
    st.where_clause = ctx->whereClause() ? std::optional<WhereClause>(buildWhereClause(ctx->whereClause())) : std::nullopt;
    st.struct_fields = buildStructFields(ctx->structField());
    return std::make_unique<StructItem>(std::move(st));
}

AstPtr<ConstantItem> AstBuilder::buildConstantItem(RxParser::ConstantItemContext* ctx) {
    ConstantItem cons(makeSpan(ctx));
    cons.name = buildIdentifier(ctx->identifier());
    cons.type = buildTypeRef(ctx->typeRef());
    cons.value = buildConstValue(ctx->constValue());
    return std::make_unique<ConstantItem>(std::move(cons));
}

IntegerSuffix AstBuilder::getSuffix(const std::string& spelling) {
    if (spelling.size() >= 3 && spelling.substr(spelling.size() - 3, 3) == "i32") {
        return IntegerSuffix::I32;
    } else if (spelling.size() >= 3 && spelling.substr(spelling.size() - 3, 3) == "u32") {
        return IntegerSuffix::U32;
    } else if (spelling.size() >= 5 && spelling.substr(spelling.size() - 5, 5) == "isize") {
        return IntegerSuffix::Isize;
    } else if (spelling.size() >= 5 && spelling.substr(spelling.size() - 5, 5) == "usize") {
        return IntegerSuffix::Usize;
    } else {
        return IntegerSuffix::None;
    }
}

AstPtr<ConstValue> AstBuilder::buildConstValue(RxParser::ConstValueContext* ctx) {
    if (ctx->INTEGER_LITERAL()) {
        return std::make_unique<ConstValue>(
            ConstValue {
                makeSpan(ctx),
                ConstValueType::Integer,
                IntegerLiteralValue {
                    makeSpan(ctx),
                    ctx->toString(),
                    getSuffix(ctx->toString())
                },
                std::nullopt,
                std::nullopt,
                nullptr,
                nullptr
            }
        );
    } else if (ctx->TRUE()) {
        return std::make_unique<ConstValue>(
            ConstValue {
                makeSpan(ctx),
                ConstValueType::Boolean,
                std::nullopt,
                true,
                std::nullopt,
                nullptr,
                nullptr
            }
        );
    } else if (ctx->FALSE()) {
        return std::make_unique<ConstValue>(
            ConstValue {
                makeSpan(ctx),
                ConstValueType::Boolean,
                std::nullopt,
                false,
                std::nullopt,
                nullptr,
                nullptr
            }
        );
    } else if (ctx->pathInExpression()) {
        return std::make_unique<ConstValue>(
            ConstValue {
                makeSpan(ctx),
                ConstValueType::ConstantPath,
                std::nullopt,
                std::nullopt,
                buildPathInExpression(ctx->pathInExpression()),
                nullptr,
                nullptr
            }
        );
    } else if (ctx->magnitude()) {
        return std::make_unique<ConstValue>(
            ConstValue {
                makeSpan(ctx),
                ConstValueType::NegatedMagnitude,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                buildMagnitude(ctx->magnitude()),
                nullptr
            }
        );
    } else if (ctx->constValue()) {
        return std::make_unique<ConstValue>(
            ConstValue {
                makeSpan(ctx),
                ConstValueType::Parenthesized,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                nullptr,
                buildConstValue(ctx->constValue())
            }
        );
    }
    return nullptr;
}

AstPtr<Magnitude> AstBuilder::buildMagnitude(RxParser::MagnitudeContext* ctx) {
    if (ctx->INTEGER_LITERAL()) {
        return std::make_unique<Magnitude>(
            Magnitude {
                makeSpan(ctx),
                MagnitudeType::IntegerLiteral,
                IntegerLiteralValue {
                    makeSpan(ctx),
                    ctx->toString(),
                    getSuffix(ctx->toString())
                },
                std::nullopt,
                nullptr
            }
        );
    } else if (ctx->pathInExpression()) {
        return std::make_unique<Magnitude>(
            Magnitude {
                makeSpan(ctx),
                MagnitudeType::IntegerLiteral,
                std::nullopt,
                buildPathInExpression(ctx->pathInExpression()),
                nullptr
            }
        );
    } else if (ctx->magnitude()) {
        return std::make_unique<Magnitude>(
            Magnitude {
                makeSpan(ctx),
                MagnitudeType::IntegerLiteral,
                std::nullopt,
                std::nullopt,
                buildMagnitude(ctx->magnitude())
            }
        );
    }
    return nullptr;
}

PathInExpression AstBuilder::buildPathInExpression(RxParser::PathInExpressionContext* ctx) {
    std::vector<PathExprSegment> segments;
    for (auto* seg: ctx->pathExprSegment()) {
        segments.push_back(buildPathExprSegment(seg));
    }
    return PathInExpression {
        makeSpan(ctx),
        segments
    };
}

PathExprSegment AstBuilder::buildPathExprSegment(RxParser::PathExprSegmentContext* ctx) {
    return PathExprSegment {
        makeSpan(ctx),
        buildPathIdentSegment(ctx->pathIdentSegment()),
        ctx->genericArgs() ? std::optional<GenericArgs>(buildGenericArgs(ctx->genericArgs())) : std::nullopt
    };
}

PathIdentSegment AstBuilder::buildPathIdentSegment(RxParser::PathIdentSegmentContext* ctx) {
    return PathIdentSegment {
        makeSpan(ctx),
        ctx->identifier() ? std::optional<std::string>(ctx->identifier()->toString()) : std::nullopt,
        ctx->SELF_VALUE() != nullptr,
        ctx->SELF_TYPE() != nullptr
    };
}

GenericArgs AstBuilder::buildGenericArgs(RxParser::GenericArgsContext* ctx) {
    std::vector<GenericArg> args;
    for (auto* arg: ctx->genericArg()) {
        args.push_back(buildGenericArg(arg));
    }
    return GenericArgs {
        makeSpan(ctx),
        args
    };
}

GenericArg AstBuilder::buildGenericArg(RxParser::GenericArgContext* ctx) {
    return GenericArg {
        makeSpan(ctx),
        ctx->lifetime() ? std::optional<Lifetime>(buildLifetime(ctx->lifetime())) : std::nullopt,
        ctx->typeRef() ? std::optional<AstPtr<TypeRef>>(buildTypeRef(ctx->typeRef())) : std::nullopt
    };
}

AstPtr<ImplItem> AstBuilder::buildImplItem(RxParser::InherentImplContext* ctx) {
    ImplItem impl(makeSpan(ctx));
    impl.generic_params = ctx->genericParams() ? buildGenericParams(ctx->genericParams()) : std::vector<GenericParam>();
    impl.type = buildTypeRef(ctx->typeRef());
    impl.where_clause = ctx->whereClause() ? std::optional<WhereClause>(buildWhereClause(ctx->whereClause())) : std::nullopt;
    impl.associated_items = buildAssociatedItems(ctx->associatedItem());
    return std::make_unique<ImplItem>(std::move(impl));
}

std::vector<AssociatedItem> AstBuilder::buildAssociatedItems(const std::vector<RxParser::AssociatedItemContext*>& ctx) {
    std::vector<AssociatedItem> items;
    for (auto* it: ctx) {
        items.push_back(buildAssociatedItem(it));
    }
    return items;
}

AssociatedItem AstBuilder::buildAssociatedItem(RxParser::AssociatedItemContext* ctx) {
    return AssociatedItem {
        ctx->constantItem() ? std::optional<AstPtr<ConstantItem>>(buildConstantItem(ctx->constantItem())) : std::nullopt,
        ctx->functionDefinition() ? std::optional<AstPtr<FunctionItem>>(buildFunctionItem(ctx->functionDefinition())) : std::nullopt
    };
}

AstPtr<TypeRef> AstBuilder::buildTypeRef(RxParser::TypeRefContext* ctx) {
    if (ctx->LPAREN() && ctx->RPAREN()) {
        if (ctx->typeRef()) {
            auto node = std::make_unique<ParenthesizedType>(ParenthesizedType(makeSpan(ctx)));
            node->type = std::move(buildTypeRef(ctx->typeRef()));
            return node;
        } else {
            return std::make_unique<TypeRef>(UnitType(makeSpan(ctx)));
        }
    } else if (ctx->typePath()) {
        return buildTypePath(ctx->typePath());
    } else if (ctx->referenceType()) {
        return buildReferenceType(ctx->referenceType());
    } else if (ctx->arrayType()) {
        return buildArrayType(ctx->arrayType());
    }
    return nullptr;
}

AstPtr<TypePath> AstBuilder::buildTypePath(RxParser::TypePathContext* ctx) {
    std::vector<TypePathSegment> segs;
    for (auto* seg: ctx->typePathSegment()) {
        segs.push_back(buildTypePathSegment(seg));
    }
    auto node = std::make_unique<TypePath>(makeSpan(ctx));
    node->path_segments = segs;
    return node;
}

TypePathSegment AstBuilder::buildTypePathSegment(RxParser::TypePathSegmentContext* ctx) {
    return TypePathSegment {
        makeSpan(ctx),
        buildPathIdentSegment(ctx->pathIdentSegment()),
        ctx->genericArgs() ? std::optional<GenericArgs>(buildGenericArgs(ctx->genericArgs())) : std::nullopt
    };
}

AstPtr<ReferenceType> AstBuilder::buildReferenceType(RxParser::ReferenceTypeContext* ctx) {
    if (ctx->ANDAND()) {
        auto inner = std::make_unique<ReferenceType>(makeSpan(ctx));
        inner->lifetime = ctx->lifetime() ? std::optional<Lifetime>(buildLifetime(ctx->lifetime())) : std::nullopt;
        inner->is_mut = ctx->MUT() != nullptr;
        inner->type = buildTypeRef(ctx->typeRef());
        auto node = std::make_unique<ReferenceType>(makeSpan(ctx));
        node->lifetime = std::nullopt;
        node->is_mut = false;
        node->type = std::move(inner);
        return node;
    } else {
        auto node = std::make_unique<ReferenceType>(ReferenceType(makeSpan(ctx)));
        node->lifetime = ctx->lifetime() ? std::optional<Lifetime>(buildLifetime(ctx->lifetime())) : std::nullopt;
        node->is_mut = ctx->MUT() != nullptr;
        node->type = buildTypeRef(ctx->typeRef());
        return node;
    }
}

AstPtr<ArrayType> AstBuilder::buildArrayType(RxParser::ArrayTypeContext* ctx) {
    auto node = std::make_unique<ArrayType>(makeSpan(ctx));
    node->type = std::move(buildTypeRef(ctx->typeRef()));
    node->length = buildConstValue(ctx->constValue());
    return node;
}

} // namespace ast