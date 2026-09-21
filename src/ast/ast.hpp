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
    bool is_mut;
    AstPtr<TypeRef> type;
};

struct Lifetime {
    SourceSpan span;
    std::string name;
};

struct SelfParam {
    SourceSpan span;
    bool is_ref;
    bool is_mut;
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
    TypeRef type;
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
    SourceSpan span;
    std::string name;
    std::vector<GenericParam> generic_params;
    std::vector<FunctionParam> function_params;
    AstPtr<TypeRef> return_type;
    std::optional<WhereClause> where_clause;
    AstPtr<BlockExpr> body;
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
    SourceSpan span;
    std::string name;
    std::vector<GenericParam> generic_params;
    std::optional<WhereClause> where_clause;
    std::vector<StructField> struct_fields;
};

struct ConstValue {
    // TODO unimplemented
};

struct ConstantItem: Item {
    SourceSpan span;
    AstPtr<TypeRef> type;
    AstPtr<ConstValue> value;
};

struct AssociatedItem {
    SourceSpan span;
    std::optional<ConstantItem> constant;
    std::optional<FunctionParam> function;
};

struct ImplItem: Item {
    SourceSpan span;
    std::vector<GenericParam> generic_params;
    AstPtr<TypeRef> type;
    std::optional<WhereClause> where_clause;
    std::vector<AssociatedItem> associated_items;
};

struct Statement: AstNode {
    using AstNode::AstNode;
};

struct Expression: AstNode {
    using AstNode::AstNode;
};

struct IdentifierBinding {
    SourceSpan span;
    bool is_mut;
    std::string name;
};

struct LetStatement: Statement {
    SourceSpan span;
    IdentifierBinding identifier_binding;
    AstPtr<TypeRef> type;
    AstPtr<Expression> expression;
};

struct ExpressionWithoutBlock {
    // TODO unimplemented
};

struct ExpressionWithBlock {
    // TODO unimplemented
};

struct ExpressionStatement: Statement {
    SourceSpan span;
    std::optional<ExpressionWithoutBlock> expression;
    std::optional<ExpressionWithBlock> block_expression;
};

struct PathIdentSegment {
    SourceSpan span;
    std::optional<std::string> name;
    bool is_self;
    bool is_Self;
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
    SourceSpan span;
    std::vector<TypePathSegment> path_segments;
};

struct UnitType: TypeRef {
    SourceSpan span;
};

struct ReferenceType: TypeRef {
    SourceSpan span;
    std::optional<Lifetime> lifetime;
    bool is_mut;
    AstPtr<TypeRef> type;
};

struct ArrayType: TypeRef {
    SourceSpan span;
    AstPtr<TypeRef> type;
    AstPtr<ConstValue> length;
};

} // namespace ast