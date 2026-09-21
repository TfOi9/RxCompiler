#pragma once
#include <cstddef>
#include <memory>
#include <vector>
#include <string>
#include <optional>

namespace ast {

struct SourceLocation {
    size_t offset;
    size_t line;
    size_t column;
};

struct SourceSpan {
    SourceLocation begin;
    SourceLocation end;
};

enum class NodeType {
    Crate,

    UseItem,
    FunctionItem,
    StructItem,
    ConstItem,
    ImplItem,

    LetStmt,
    ExprStmt,
    EmptyStmt,

    IntegerExpr,
    BoolExpr,
    PathExpr,
    UnaryExpr,
    BinaryExpr,
    AssignExpr,
    CastExpr,
    CallExpr,
    IndexExpr,
    FieldExpr,
    BlockExpr,
    IfExpr,
    LoopExpr,
    WhileExpr,
    BreakExpr,
    ReturnExpr,
    ContinueExpr,
    ArrayExpr,
    StructExpr,
    UnitExpr,

    UnitType,
    PathType,
    ReferenceType,
    ArrayType
};

struct AstNode {
    SourceSpan span;
    NodeType type;

    AstNode(SourceSpan span, NodeType type): span(span), type(type) {}
    virtual ~AstNode() = default;
};

template <typename T>
using AstPtr = std::unique_ptr<T>;

struct Item: AstNode {
    using AstNode::AstNode;
};

struct Crate: AstNode {
    std::vector<AstPtr<Item>> items;

    explicit Crate(SourceSpan span): AstNode(span, NodeType::Crate) {}
};

struct TypeRef: AstNode {
    using AstNode::AstNode;
};

struct FunctionParam {
    SourceSpan span;
    std::string name;
    bool is_mut = 0;
    AstPtr<TypeRef> type;
};

struct Lifetime {
    SourceSpan span;
    std::string name;
};

struct SelfParam {
    SourceSpan span;
    bool is_ref = 0;
    bool is_mut = 0;
    std::optional<Lifetime> lifetime;
};

struct LifetimeBounds {
    SourceSpan span;
    std::vector<Lifetime> lifetimes;
};

struct TypeParamBounds {
    SourceSpan span;
    std::vector<Lifetime> lifetimes;
};

struct GenericParam {
    SourceSpan span;
    Lifetime lifetime;
    std::optional<LifetimeBounds> lifetime_bounds;
};

struct LifetimeWhereClauseItem {
    SourceSpan span;
    Lifetime lifetime;
    LifetimeBounds lifetime_bounds;
};

struct TypeBoundWhereClauseItem {
    SourceSpan span;
    AstPtr<TypeRef> type;
    TypeParamBounds type_param_bounds;
};

struct WhereClauseItem {
    SourceSpan span;
    std::optional<LifetimeWhereClauseItem> lifetime;
    std::optional<TypeBoundWhereClauseItem> type_bound;
};

struct WhereClause {
    SourceSpan span;
    std::vector<WhereClauseItem> items;
};

struct BlockExpr;

struct FunctionItem: Item {
    std::string name;
    std::vector<GenericParam> generic_params;
    std::optional<SelfParam> self_param;
    std::vector<FunctionParam> function_params;
    AstPtr<TypeRef> return_type;
    std::optional<WhereClause> where_clause;
    AstPtr<BlockExpr> body;

    explicit FunctionItem(SourceSpan span): Item(span, NodeType::FunctionItem) {} 
};

enum class DeriveName {
    Copy,
    Clone,
    PartialEq,
    Eq
};

struct OuterAttribute {
    SourceSpan span;
    std::vector<DeriveName> derive_names;
};

struct StructField {
    SourceSpan span;
    std::string name;
    AstPtr<TypeRef> type;
};

struct StructItem: Item {
    std::vector<OuterAttribute> attributes;
    std::string name;
    std::vector<GenericParam> generic_params;
    std::optional<WhereClause> where_clause;
    std::vector<StructField> struct_fields;

    explicit StructItem(SourceSpan span): Item(span, NodeType::StructItem) {}
};

struct ConstValue {
    // TODO unimplemented
};

struct ConstantItem: Item {
    std::string name;
    AstPtr<TypeRef> type;
    AstPtr<ConstValue> value;

    explicit ConstantItem(SourceSpan span): Item(span, NodeType::ConstItem) {}
};

struct AssociatedItem {
    std::optional<AstPtr<ConstantItem>> constant;
    std::optional<AstPtr<FunctionItem>> function;
};

struct ImplItem: Item {
    std::vector<GenericParam> generic_params;
    AstPtr<TypeRef> type;
    std::optional<WhereClause> where_clause;
    std::vector<AssociatedItem> associated_items;

    explicit ImplItem(SourceSpan span): Item(span, NodeType::ImplItem) {}
};

struct Statement: AstNode {
    using AstNode::AstNode;
};

struct Expression: AstNode {
    using AstNode::AstNode;
};

struct IdentifierBinding {
    SourceSpan span;
    bool is_mut = 0;
    std::string name;
};

struct LetStatement: Statement {
    IdentifierBinding identifier_binding;
    AstPtr<TypeRef> type;
    AstPtr<Expression> expression;

    explicit LetStatement(SourceSpan span): Statement(span, NodeType::LetStmt) {}
};

struct ExpressionWithoutBlock {
    // TODO unimplemented
};

struct ExpressionWithBlock {
    // TODO unimplemented
};

struct ExpressionStatement: Statement {
    std::optional<ExpressionWithoutBlock> expression;
    std::optional<ExpressionWithBlock> block_expression;

    explicit ExpressionStatement(SourceSpan span): Statement(span, NodeType::ExprStmt) {}
};

struct PathIdentSegment {
    SourceSpan span;
    std::optional<std::string> name;
    bool is_self = 0;
    bool is_Self = 0;
};

struct GenericArg {
    SourceSpan span;
    std::optional<Lifetime> lifetime;
    std::optional<AstPtr<TypeRef>> type;
};
struct GenericArgs {
    SourceSpan span;
    std::vector<GenericArg> args;
};

struct PathExprSegment {
    SourceSpan span;
    PathIdentSegment ident_segment;
    std::optional<GenericArgs> generic_args;
};

struct PathInExpression {
    SourceSpan span;
    std::vector<PathExprSegment> segments;
};

struct TypePathSegment {
    SourceSpan span;
    PathIdentSegment ident_segment;
    std::optional<GenericArgs> generic_args;
};

struct TypePath: TypeRef {
    std::vector<TypePathSegment> path_segments;

    explicit TypePath(SourceSpan span): TypeRef(span, NodeType::PathType) {}
};

struct UnitType: TypeRef {
    explicit UnitType(SourceSpan span): TypeRef(span, NodeType::UnitType) {}
};

struct ReferenceType: TypeRef {
    std::optional<Lifetime> lifetime;
    bool is_mut = 0;
    AstPtr<TypeRef> type;

    explicit ReferenceType(SourceSpan span): TypeRef(span, NodeType::ReferenceType) {}
};

struct ArrayType: TypeRef {
    AstPtr<TypeRef> type;
    AstPtr<ConstValue> length;

    explicit ArrayType(SourceSpan span): TypeRef(span, NodeType::ArrayType) {}
};

} // namespace ast
