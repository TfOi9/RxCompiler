#include "builder.hpp"
#include "ast.hpp"
#include <memory>
#include <optional>
#include <stdexcept>
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
        ctx->MUT() != nullptr,
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
            ctx->typeParamBounds() ? std::optional<TypeParamBounds>(buildTypeParamBounds(ctx->typeParamBounds())) : std::nullopt
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
    fn.self_param = (ctx->functionParameters() && ctx->functionParameters()->selfParam()) ? std::optional<SelfParam>(buildSelfParam(ctx->functionParameters()->selfParam())) : std::nullopt;
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
                MagnitudeType::ConstantPath,
                std::nullopt,
                buildPathInExpression(ctx->pathInExpression()),
                nullptr
            }
        );
    } else if (ctx->magnitude()) {
        return std::make_unique<Magnitude>(
            Magnitude {
                makeSpan(ctx),
                MagnitudeType::Parenthesized,
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
        std::move(segments)
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
        std::move(args)
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
            node->type = buildTypeRef(ctx->typeRef());
            return node;
        } else {
            return std::make_unique<UnitType>(UnitType(makeSpan(ctx)));
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
    node->path_segments = std::move(segs);
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
    node->type = buildTypeRef(ctx->typeRef());
    node->length = buildConstValue(ctx->constValue());
    return node;
}

AstPtr<Statement> AstBuilder::buildStatement(RxParser::StatementContext* ctx) {
    if (ctx->letStatement()) {
        return buildLetStatement(ctx->letStatement());
    } else if (ctx->expressionWithBlock()) {
        auto node = std::make_unique<ExpressionStatement>(ExpressionStatement(makeSpan(ctx)));
        node->expr = buildExpressionWithBlock(ctx->expressionWithBlock());
        return node;
    } else if (ctx->statementExpression()) {
        auto node = std::make_unique<ExpressionStatement>(ExpressionStatement(makeSpan(ctx)));
        node->expr = buildStatementExpression(ctx->statementExpression());
        return node;
    } else {
        return std::make_unique<EmptyStatement>(EmptyStatement(makeSpan(ctx)));
    }
}

AstPtr<LetStatement> AstBuilder::buildLetStatement(RxParser::LetStatementContext* ctx) {
    auto node = std::make_unique<LetStatement>(LetStatement(makeSpan(ctx)));
    node->identifier_binding = buildIdentifierBinding(ctx->identifierBinding());
    node->type = ctx->typeRef() ? buildTypeRef(ctx->typeRef()) : nullptr;
    node->expression = buildExpression(ctx->expression());
    return node;
}

AstPtr<Expression> AstBuilder::buildExpressionWithBlock(RxParser::ExpressionWithBlockContext* ctx) {
    if (ctx->ifExpression()) {
        return buildIfExpression(ctx->ifExpression());
    } else if (ctx->LOOP()) {
        return buildLoopExpression(ctx);
    } else if (ctx->WHILE()) {
        return buildWhileExpression(ctx);
    } else if (ctx->blockExpression()) {
        return buildBlockExpression(ctx->blockExpression());
    }
    return nullptr;
}

AstPtr<BlockExpression> AstBuilder::buildBlockExpression(RxParser::BlockExpressionContext* ctx) {
    auto node = std::make_unique<BlockExpression>(BlockExpression(makeSpan(ctx)));
    for (auto* stmt: ctx->statement()) {
        node->statements.push_back(buildStatement(stmt));
    }
    node->tail_expression = ctx->statementExpression() ? buildStatementExpression(ctx->statementExpression()) : nullptr;
    return node;
}

AstPtr<IfExpression> AstBuilder::buildIfExpression(RxParser::IfExpressionContext* ctx) {
    auto node = std::make_unique<IfExpression>(IfExpression(makeSpan(ctx)));
    node->condition = buildConditionExpression(ctx->conditionExpression());
    node->then_block = buildBlockExpression(ctx->blockExpression()[0]);
    if (ctx->ELSE()) {
        if (ctx->ifExpression()) {
            node->else_branch = buildIfExpression(ctx->ifExpression());
        } else if (ctx->blockExpression().size() > 1) {
            node->else_branch = buildBlockExpression(ctx->blockExpression()[1]);
        } else {
            node->else_branch = nullptr;
        }
    } else {
        node->else_branch = nullptr;
    }
    return node;
}

AstPtr<LoopExpression> AstBuilder::buildLoopExpression(RxParser::ExpressionWithBlockContext* ctx) {
    auto node = std::make_unique<LoopExpression>(LoopExpression(makeSpan(ctx)));
    node->body = buildBlockExpression(ctx->blockExpression());
    return node;
}

AstPtr<WhileExpression> AstBuilder::buildWhileExpression(RxParser::ExpressionWithBlockContext* ctx) {
    auto node = std::make_unique<WhileExpression>(WhileExpression(makeSpan(ctx)));
    node->condition = buildConditionExpression(ctx->conditionExpression());
    node->body = buildBlockExpression(ctx->blockExpression());
    return node;
}

AstPtr<Expression> AstBuilder::buildExpression(RxParser::ExpressionContext* ctx) {
    if (ctx->assignmentExpression()) {
        return buildAssignmentExpression(ctx->assignmentExpression());
    } else {
        return nullptr;
    }
}

AstPtr<Expression> AstBuilder::buildAssignmentExpression(RxParser::AssignmentExpressionContext* ctx) {
    auto lhs = buildLogicalOrExpression(ctx->logicalOrExpression());
    if (!ctx->assignmentOperator()) {
        return lhs;
    }
    auto* assignCtx = ctx->assignmentOperator();
    AssignmentOperator op;
    if (assignCtx->PLUS_ASSIGN()) {
        op = AssignmentOperator::AssignAdd;
    } else if (assignCtx->MINUS_ASSIGN()) {
        op = AssignmentOperator::AssignSubtract;
    } else if (assignCtx->STAR_ASSIGN()) {
        op = AssignmentOperator::AssignMultiply;
    } else if (assignCtx->SLASH_ASSIGN()) {
        op = AssignmentOperator::AssignDivide;
    } else if (assignCtx->PERCENT_ASSIGN()) {
        op = AssignmentOperator::AssignRemainder;
    } else if (assignCtx->AMP_ASSIGN()) {
        op = AssignmentOperator::AssignBitwiseAnd;
    } else if (assignCtx->PIPE_ASSIGN()) {
        op = AssignmentOperator::AssignBitwiseOr;
    } else if (assignCtx->CARET_ASSIGN()) {
        op = AssignmentOperator::AssignBitwiseXor;
    } else if (assignCtx->SHL_ASSIGN()) {
        op = AssignmentOperator::AssignShiftLeft;
    } else if (assignCtx->GT()) {
        op = AssignmentOperator::AssignShiftRight;
    } else {
        op = AssignmentOperator::Assign;
    }
    auto rhs = buildExpression(ctx->expression());
    auto node = std::make_unique<AssignmentExpression>(AssignmentExpression(makeSpan(ctx)));
    node->lhs_operand = std::move(lhs);
    node->op = op;
    node->rhs_operand = std::move(rhs);
    return node;
}

AstPtr<Expression> AstBuilder::buildLogicalOrExpression(RxParser::LogicalOrExpressionContext* ctx) {
    const auto operands = ctx->logicalAndExpression();
    const auto operators = ctx->OROR();
    auto lhs = buildLogicalAndExpression(operands[0]);
    for (size_t i = 0; i < operators.size(); i++) {
        auto rhs = buildLogicalAndExpression(operands[i + 1]);
        SourceSpan span {lhs->span.begin, rhs->span.end};
        auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
        node->lhs_operand = std::move(lhs);
        node->op = BinaryOperator::LogicalOr;
        node->rhs_operand = std::move(rhs);
        lhs = std::move(node);
    }
    return lhs;
}

AstPtr<Expression> AstBuilder::buildLogicalAndExpression(RxParser::LogicalAndExpressionContext* ctx) {
    const auto operands = ctx->comparisonExpression();
    const auto operators = ctx->ANDAND();
    auto lhs = buildComparisonExpression(operands[0]);
    for (size_t i = 0; i < operators.size(); i++) {
        auto rhs = buildComparisonExpression(operands[i + 1]);
        SourceSpan span {lhs->span.begin, rhs->span.end};
        auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
        node->lhs_operand = std::move(lhs);
        node->op = BinaryOperator::LogicalAnd;
        node->rhs_operand = std::move(rhs);
        lhs = std::move(node);
    }
    return lhs;
}

AstPtr<Expression> AstBuilder::buildComparisonExpression(RxParser::ComparisonExpressionContext* ctx) {
    AstPtr<Expression> lhs, rhs;
    BinaryOperator op;
    if (ctx->LT()) {
        lhs = buildClosedBitOrExpression(ctx->closedBitOrExpression());
        rhs = buildBitOrExpression(ctx->bitOrExpression()[0]);
        op = BinaryOperator::Less;
    } else {
        lhs = buildBitOrExpression(ctx->bitOrExpression()[0]);
        if (!ctx->comparisonExceptLt()) {
            return lhs;
        } else {
            op = buildComparisonExceptLtContext(ctx->comparisonExceptLt());
            rhs = buildBitOrExpression(ctx->bitOrExpression()[1]);
        }
    }
    auto node = std::make_unique<BinaryExpression>(BinaryExpression(makeSpan(ctx)));
    node->lhs_operand = std::move(lhs);
    node->op = op;
    node->rhs_operand = std::move(rhs);
    return node;
}

BinaryOperator AstBuilder::buildComparisonExceptLtContext(RxParser::ComparisonExceptLtContext* ctx) {
    if (ctx->EQEQ()) {
        return BinaryOperator::Equal;
    } else if (ctx->NE()) {
        return BinaryOperator::NotEqual;
    } else if (ctx->LE()) {
        return BinaryOperator::LessEqual;
    } else if (ctx->GT() && ctx->GE_EQ()) {
        return BinaryOperator::GreaterEqual;
    } else if (ctx->GT_SECOND() && ctx->SHR_EQ()) {
        return BinaryOperator::GreaterEqual;
    } else if (ctx->genericClose()) {
        return BinaryOperator::Greater;
    }
    throw std::logic_error("unexpected comparison operator");
}

AstPtr<Expression> AstBuilder::buildClosedBitOrExpression(RxParser::ClosedBitOrExpressionContext* ctx) {
    const auto operands = ctx->bitXorExpression();
    if (operands.empty()) {
        return buildClosedBitXorExpression(ctx->closedBitXorExpression());
    }
    AstPtr<Expression> lhs, rhs;
    lhs = buildBitXorExpression(operands[0]);
    for (size_t i = 1; i < operands.size(); i++) {
        rhs = buildBitXorExpression(operands[i]);
        SourceSpan span {lhs->span.begin, rhs->span.end};
        auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
        node->lhs_operand = std::move(lhs);
        node->op = BinaryOperator::BitwiseOr;
        node->rhs_operand = std::move(rhs);
        lhs = std::move(node);
    }
    rhs = buildClosedBitXorExpression(ctx->closedBitXorExpression());
    SourceSpan span {lhs->span.begin, rhs->span.end};
    auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
    node->lhs_operand = std::move(lhs);
    node->op = BinaryOperator::BitwiseOr;
    node->rhs_operand = std::move(rhs);
    return node;
}

AstPtr<Expression> AstBuilder::buildBitOrExpression(RxParser::BitOrExpressionContext* ctx) {
    const auto operands = ctx->bitXorExpression();
    AstPtr<Expression> lhs, rhs;
    lhs = buildBitXorExpression(operands[0]);
    for (size_t i = 1; i < operands.size(); i++) {
        rhs = buildBitXorExpression(operands[i]);
        SourceSpan span {lhs->span.begin, rhs->span.end};
        auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
        node->lhs_operand = std::move(lhs);
        node->op = BinaryOperator::BitwiseOr;
        node->rhs_operand = std::move(rhs);
        lhs = std::move(node);
    }
    return lhs;
}

AstPtr<Expression> AstBuilder::buildClosedBitXorExpression(RxParser::ClosedBitXorExpressionContext* ctx) {
    const auto operands = ctx->bitAndExpression();
    if (operands.empty()) {
        return buildClosedBitAndExpression(ctx->closedBitAndExpression());
    }
    AstPtr<Expression> lhs, rhs;
    lhs = buildBitAndExpression(operands[0]);
    for (size_t i = 1; i < operands.size(); i++) {
        rhs = buildBitAndExpression(operands[i]);
        SourceSpan span {lhs->span.begin, rhs->span.end};
        auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
        node->lhs_operand = std::move(lhs);
        node->op = BinaryOperator::BitwiseXor;
        node->rhs_operand = std::move(rhs);
        lhs = std::move(node);
    }
    rhs = buildClosedBitAndExpression(ctx->closedBitAndExpression());
    SourceSpan span {lhs->span.begin, rhs->span.end};
    auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
    node->lhs_operand = std::move(lhs);
    node->op = BinaryOperator::BitwiseXor;
    node->rhs_operand = std::move(rhs);
    return node;
}

AstPtr<Expression> AstBuilder::buildBitXorExpression(RxParser::BitXorExpressionContext* ctx) {
    const auto operands = ctx->bitAndExpression();
    AstPtr<Expression> lhs, rhs;
    lhs = buildBitAndExpression(operands[0]);
    for (size_t i = 1; i < operands.size(); i++) {
        rhs = buildBitAndExpression(operands[i]);
        SourceSpan span {lhs->span.begin, rhs->span.end};
        auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
        node->lhs_operand = std::move(lhs);
        node->op = BinaryOperator::BitwiseXor;
        node->rhs_operand = std::move(rhs);
        lhs = std::move(node);
    }
    return lhs;
}

AstPtr<Expression> AstBuilder::buildClosedBitAndExpression(RxParser::ClosedBitAndExpressionContext* ctx) {
    const auto operands = ctx->shiftExpression();
    if (operands.empty()) {
        return buildClosedShiftExpression(ctx->closedShiftExpression());
    }
    AstPtr<Expression> lhs, rhs;
    lhs = buildShiftExpression(operands[0]);
    for (size_t i = 1; i < operands.size(); i++) {
        rhs = buildShiftExpression(operands[i]);
        SourceSpan span {lhs->span.begin, rhs->span.end};
        auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
        node->lhs_operand = std::move(lhs);
        node->op = BinaryOperator::BitwiseAnd;
        node->rhs_operand = std::move(rhs);
        lhs = std::move(node);
    }
    rhs = buildClosedShiftExpression(ctx->closedShiftExpression());
    SourceSpan span {lhs->span.begin, rhs->span.end};
    auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
    node->lhs_operand = std::move(lhs);
    node->op = BinaryOperator::BitwiseAnd;
    node->rhs_operand = std::move(rhs);
    return node;
}

AstPtr<Expression> AstBuilder::buildBitAndExpression(RxParser::BitAndExpressionContext* ctx) {
    const auto operands = ctx->shiftExpression();
    AstPtr<Expression> lhs, rhs;
    lhs = buildShiftExpression(operands[0]);
    for (size_t i = 1; i < operands.size(); i++) {
        rhs = buildShiftExpression(operands[i]);
        SourceSpan span {lhs->span.begin, rhs->span.end};
        auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
        node->lhs_operand = std::move(lhs);
        node->op = BinaryOperator::BitwiseAnd;
        node->rhs_operand = std::move(rhs);
        lhs = std::move(node);
    }
    return lhs;
}

AstPtr<Expression> AstBuilder::buildClosedShiftExpression(RxParser::ClosedShiftExpressionContext* ctx) {
    AstPtr<Expression> lhs;
    std::optional<BinaryOperator> pending_op;
    for (auto* child: ctx->children) {
        AstPtr<Expression> operand;
        if (auto* closed_add = dynamic_cast<RxParser::ClosedAdditiveExpressionContext*>(child)) {
            operand = buildClosedAdditiveExpression(closed_add);
        } else if (auto* add = dynamic_cast<RxParser::AdditiveExpressionContext*>(child)) {
            operand = buildAdditiveExpression(add);
        }
        if (operand) {
            if (!lhs) {
                lhs = std::move(operand);
                continue;
            }
            if (!pending_op) {
                continue;
            }
            SourceSpan span {lhs->span.begin, operand->span.end};
            auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
            node->lhs_operand = std::move(lhs);
            node->op = pending_op.value();
            node->rhs_operand = std::move(operand);
            lhs = std::move(node);
            pending_op.reset();
            continue;
        }
        if (dynamic_cast<RxParser::ShiftRightContext*>(child)) {
            pending_op = BinaryOperator::ShiftRight;
        }
        if (auto* terminal = dynamic_cast<antlr4::tree::TerminalNode*>(child)) {
            if (terminal && terminal->getSymbol()->getType() == RxParser::SHL) {
                pending_op = BinaryOperator::ShiftLeft;
            }
        }
    }
    if (pending_op) {
        return nullptr;
    }
    return lhs;
}

AstPtr<Expression> AstBuilder::buildShiftExpression(RxParser::ShiftExpressionContext* ctx) {
    AstPtr<Expression> lhs;
    std::optional<BinaryOperator> pending_op;
    for (auto* child: ctx->children) {
        AstPtr<Expression> operand;
        if (auto* closed_add = dynamic_cast<RxParser::ClosedAdditiveExpressionContext*>(child)) {
            operand = buildClosedAdditiveExpression(closed_add);
        } else if (auto* add = dynamic_cast<RxParser::AdditiveExpressionContext*>(child)) {
            operand = buildAdditiveExpression(add);
        }
        if (operand) {
            if (!lhs) {
                lhs = std::move(operand);
                continue;
            }
            if (!pending_op) {
                continue;
            }
            SourceSpan span {lhs->span.begin, operand->span.end};
            auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
            node->lhs_operand = std::move(lhs);
            node->op = pending_op.value();
            node->rhs_operand = std::move(operand);
            lhs = std::move(node);
            pending_op.reset();
            continue;
        }
        if (dynamic_cast<RxParser::ShiftRightContext*>(child)) {
            pending_op = BinaryOperator::ShiftRight;
        }
        if (auto* terminal = dynamic_cast<antlr4::tree::TerminalNode*>(child)) {
            if (terminal && terminal->getSymbol()->getType() == RxParser::SHL) {
                pending_op = BinaryOperator::ShiftLeft;
            }
        }
    }
    if (pending_op) {
        return nullptr;
    }
    return lhs;
}

AstPtr<Expression> AstBuilder::buildClosedAdditiveExpression(RxParser::ClosedAdditiveExpressionContext* ctx) {
    const auto operands = ctx->multiplicativeExpression();
    if (operands.empty()) {
        return buildClosedMultiplicativeExpression(ctx->closedMultiplicativeExpression());
    }
    AstPtr<Expression> lhs, rhs;
    lhs = buildMultiplicativeExpression(operands[0]);
    for (size_t i = 1; i < operands.size(); i++) {
        rhs = buildMultiplicativeExpression(operands[i]);
        SourceSpan span {lhs->span.begin, rhs->span.end};
        auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
        node->lhs_operand = std::move(lhs);
        node->op = buildAdditiveOperator(ctx->additiveOperator()[i - 1]);
        node->rhs_operand = std::move(rhs);
        lhs = std::move(node);
    }
    rhs = buildClosedMultiplicativeExpression(ctx->closedMultiplicativeExpression());
    SourceSpan span {lhs->span.begin, rhs->span.end};
    auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
    node->lhs_operand = std::move(lhs);
    node->op = buildAdditiveOperator(ctx->additiveOperator()[operands.size() - 1]);
    node->rhs_operand = std::move(rhs);
    return node;
}

AstPtr<Expression> AstBuilder::buildAdditiveExpression(RxParser::AdditiveExpressionContext* ctx) {
    const auto operands = ctx->multiplicativeExpression();
    AstPtr<Expression> lhs, rhs;
    lhs = buildMultiplicativeExpression(operands[0]);
    for (size_t i = 1; i < operands.size(); i++) {
        rhs = buildMultiplicativeExpression(operands[i]);
        SourceSpan span {lhs->span.begin, rhs->span.end};
        auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
        node->lhs_operand = std::move(lhs);
        node->op = buildAdditiveOperator(ctx->additiveOperator()[i - 1]);
        node->rhs_operand = std::move(rhs);
        lhs = std::move(node);
    }
    return lhs;
}

BinaryOperator AstBuilder::buildAdditiveOperator(RxParser::AdditiveOperatorContext* ctx) {
    if (ctx->PLUS()) {
        return BinaryOperator::Add;
    } else if (ctx->MINUS()) {
        return BinaryOperator::Subtract;
    }
    throw std::logic_error("unexpected additive operator");
}

AstPtr<Expression> AstBuilder::buildClosedMultiplicativeExpression(RxParser::ClosedMultiplicativeExpressionContext* ctx) {
    const auto operands = ctx->castExpression();
    if (operands.empty()) {
        return buildClosedCastExpression(ctx->closedCastExpression());
    }
    AstPtr<Expression> lhs, rhs;
    lhs = buildCastExpression(operands[0]);
    for (size_t i = 1; i < operands.size(); i++) {
        rhs = buildCastExpression(operands[i]);
        SourceSpan span {lhs->span.begin, rhs->span.end};
        auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
        node->lhs_operand = std::move(lhs);
        node->op = buildMultiplicativeOperator(ctx->multiplicativeOperator()[i - 1]);
        node->rhs_operand = std::move(rhs);
        lhs = std::move(node);
    }
    rhs = buildClosedCastExpression(ctx->closedCastExpression());
    SourceSpan span {lhs->span.begin, rhs->span.end};
    auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
    node->lhs_operand = std::move(lhs);
    node->op = buildMultiplicativeOperator(ctx->multiplicativeOperator()[operands.size() - 1]);
    node->rhs_operand = std::move(rhs);
    return node;
}

AstPtr<Expression> AstBuilder::buildMultiplicativeExpression(RxParser::MultiplicativeExpressionContext* ctx) {
    const auto operands = ctx->castExpression();
    AstPtr<Expression> lhs, rhs;
    lhs = buildCastExpression(operands[0]);
    for (size_t i = 1; i < operands.size(); i++) {
        rhs = buildCastExpression(operands[i]);
        SourceSpan span {lhs->span.begin, rhs->span.end};
        auto node = std::make_unique<BinaryExpression>(BinaryExpression(span));
        node->lhs_operand = std::move(lhs);
        node->op = buildMultiplicativeOperator(ctx->multiplicativeOperator()[i - 1]);
        node->rhs_operand = std::move(rhs);
        lhs = std::move(node);
    }
    return lhs;
}

BinaryOperator AstBuilder::buildMultiplicativeOperator(RxParser::MultiplicativeOperatorContext* ctx) {
    if (ctx->STAR()) {
        return BinaryOperator::Multiply;
    } else if (ctx->SLASH()) {
        return BinaryOperator::Divide;
    } else if (ctx->PERCENT()) {
        return BinaryOperator::Remainder;
    }
    throw std::logic_error("unexpected multiplicative operator");
}

AstPtr<Expression> AstBuilder::buildClosedCastExpression(RxParser::ClosedCastExpressionContext* ctx) {
    if (ctx->unaryExpression()) {
        return buildUnaryExpression(ctx->unaryExpression());
    }
    auto node = std::make_unique<CastExpression>(CastExpression(makeSpan(ctx)));
    node->operand = std::move(buildCastExpression(ctx->castExpression()));
    node->target_type = std::move(buildClosedCastType(ctx->closedCastType()));
    return node;
}

AstPtr<TypeRef> AstBuilder::buildClosedCastType(RxParser::ClosedCastTypeContext* ctx) {
    if (ctx->LPAREN() && ctx->RPAREN()) {
        if (ctx->typeRef()) {
            return buildTypeRef(ctx->typeRef());
        } else {
            return std::make_unique<UnitType>(UnitType(makeSpan(ctx)));
        }
    } else if (ctx->arrayType()) {
        return buildArrayType(ctx->arrayType());
    } else if (ctx->closedCastType()) {
        if (ctx->AMP()) {
            auto node = std::make_unique<ReferenceType>(ReferenceType(makeSpan(ctx)));
            node->type = buildClosedCastType(ctx->closedCastType());
            node->lifetime = ctx->lifetime() ? std::optional<Lifetime>(buildLifetime(ctx->lifetime())) : std::nullopt;
            node->is_mut = ctx->MUT() != nullptr;
            return std::move(node);
        } else if (ctx->ANDAND()) {
            auto inner = std::make_unique<ReferenceType>(ReferenceType(makeSpan(ctx)));
            inner->type = buildClosedCastType(ctx->closedCastType());
            inner->lifetime = ctx->lifetime() ? std::optional<Lifetime>(buildLifetime(ctx->lifetime())) : std::nullopt;
            inner->is_mut = ctx->MUT() != nullptr;
            auto node = std::make_unique<ReferenceType>(ReferenceType(makeSpan(ctx)));
            node->type = std::move(inner);
            node->lifetime = std::nullopt;
            node->is_mut = false;
            return std::move(node);
        } else {
            throw std::logic_error("missing & or &&");
        }
    } else if (ctx->pathIdentSegment()) {
        auto node = std::make_unique<TypePath>(TypePath(makeSpan(ctx)));
        for (auto* seg: ctx->typePathSegment()) {
            node->path_segments.push_back(buildTypePathSegment(seg));
        }
        auto* ident_ctx = ctx->pathIdentSegment();
        auto* args_ctx = ctx->genericArgs();
        if (!ident_ctx || !args_ctx) {
            throw std::logic_error("incomplete closed type path");
        }
        const auto ident_span = makeSpan(ident_ctx);
        const auto args_span = makeSpan(args_ctx);
        node->path_segments.push_back(TypePathSegment {
            SourceSpan {ident_span.begin, args_span.end},
            buildPathIdentSegment(ident_ctx),
            buildGenericArgs(args_ctx)
        });
        return node;
    }
    throw std::logic_error("unexpected closed cast type");
}

AstPtr<Expression> AstBuilder::buildCastExpression(RxParser::CastExpressionContext* ctx) {
    AstPtr<Expression> lhs = std::move(buildUnaryExpression(ctx->unaryExpression()));
    for (auto* type: ctx->typeRef()) {
        AstPtr<TypeRef> target_type = buildTypeRef(type);
        SourceSpan span {lhs->span.begin, target_type->span.end};
        auto node = std::make_unique<CastExpression>(CastExpression(span));
        node->operand = std::move(lhs);
        node->target_type = std::move(target_type);
        lhs = std::move(node);
    }
    return lhs;
}

AstPtr<Expression> AstBuilder::buildUnaryExpression(RxParser::UnaryExpressionContext* ctx) {
    if (ctx->postfixExpression()) {
        return buildPostfixExpression(ctx->postfixExpression());
    }
    auto node = std::make_unique<UnaryExpression>(UnaryExpression(makeSpan(ctx)));
    if (ctx->unaryOperator()->ANDAND()) {
        auto inner = std::make_unique<UnaryExpression>(UnaryExpression(makeSpan(ctx)));
        inner->operand = buildUnaryExpression(ctx->unaryExpression());
        if (ctx->unaryOperator()->MUT()) {
            inner->op = UnaryOperator::BorrowMut;
        } else {
            inner->op = UnaryOperator::Borrow;
        }
        node->operand = std::move(inner);
        node->op = UnaryOperator::Borrow;
    } else {
        node->op = buildUnaryOperator(ctx->unaryOperator());
        node->operand = buildUnaryExpression(ctx->unaryExpression());
    }
    return std::move(node);
}

UnaryOperator AstBuilder::buildUnaryOperator(RxParser::UnaryOperatorContext* ctx) {
    if (ctx->AMP()) {
        return UnaryOperator::Borrow;
    } else if (ctx->ANDAND()) {
        throw std::logic_error("unexprected unary operator type");
    } else if (ctx->MINUS()) {
        return UnaryOperator::Negation;
    } else if (ctx->NOT()) {
        return UnaryOperator::Not;
    } else if (ctx->MUT() && ctx->AMP()) {
        return UnaryOperator::BorrowMut;
    } else if (ctx->STAR()) {
        return UnaryOperator::Dereference;
    }
    throw std::logic_error("unexprected unary operator type");
}

AstPtr<Expression> AstBuilder::buildPostfixExpression(RxParser::PostfixExpressionContext* ctx) {
    AstPtr<Expression> value = buildPrimaryExpression(ctx->primaryExpression());
    for (auto* suffix: ctx->postfixSuffix()) {
        SourceSpan span{value->span.begin, makeSpan(suffix).end};
        if (auto* args = suffix->callArguments()) {
            auto node = std::make_unique<CallExpression>(span);
            node->callee = std::move(value);
            for (auto* arg: args->expression()) {
                node->args.push_back(buildExpression(arg));
            }
            value = std::move(node);
        } else if (suffix->LBRACKET()) {
            auto node = std::make_unique<IndexExpression>(span);
            node->base = std::move(value);
            node->index = buildExpression(suffix->expression());
            value = std::move(node);
        } else {
            auto dot = suffix->dotSuffix();
            if (dot->identifier()) {
                auto node = std::make_unique<FieldExpression>(span);
                node->base = std::move(value);
                node->field_name = buildIdentifier(dot->identifier());
                value = std::move(node);
            } else {
                auto node = std::make_unique<MethodCallExpression>(span);
                node->receiver = std::move(value);
                node->method = buildPathExprSegment(dot->pathExprSegment());
                value = std::move(node);
            }
        }
    }
    return value;
}

AstPtr<Expression> AstBuilder::buildPrimaryExpression(RxParser::PrimaryExpressionContext* ctx) {
    if (ctx->expressionWithBlock()) {
        return buildExpressionWithBlock(ctx->expressionWithBlock());
    } else {
        auto* non_block = ctx->nonBlockPrimary();
        if (non_block->literalExpression()) {
            return buildLiteralExpression(non_block->literalExpression());
        } else if (non_block->pathInExpression()) {
            return buildPathOrStructExpression(non_block);
        } else if (non_block->LPAREN()) {
            if (non_block->expression()) {
                return buildExpression(non_block->expression());
            } else {
                return std::make_unique<UnitExpression>(UnitExpression(makeSpan(ctx)));
            }
        } else if (non_block->arrayExpression()) {
            return buildArrayExpression(non_block->arrayExpression());
        } else if (non_block->BREAK()) {
            auto node = std::make_unique<BreakExpression>(BreakExpression(makeSpan(ctx)));
            node->expr = non_block->expression() ? buildExpression(non_block->expression()) : nullptr;
            return node;
        } else if (non_block->RETURN()) {
            auto node = std::make_unique<ReturnExpression>(ReturnExpression(makeSpan(ctx)));
            node->expr = non_block->expression() ? buildExpression(non_block->expression()) : nullptr;
            return node;
        } else if (non_block->CONTINUE()) {
            return std::make_unique<ContinueExpression>(ContinueExpression(makeSpan(ctx)));
        }
        throw std::logic_error("unexpected expression type");
    }
}

} // namespace ast