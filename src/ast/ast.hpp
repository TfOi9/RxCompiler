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
    MethodCallExpr,
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
    GroupedExpr,

    UnitType,
    PathType,
    ReferenceType,
    ArrayType,
    ParenthesizedType
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
    std::optional<TypeParamBounds> type_param_bounds;
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

struct BlockExpression;

struct FunctionItem: Item {
    std::string name;
    std::vector<GenericParam> generic_params;
    std::optional<SelfParam> self_param;
    std::vector<FunctionParam> function_params;
    AstPtr<TypeRef> return_type;
    std::optional<WhereClause> where_clause;
    AstPtr<BlockExpression> body;

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

enum class MagnitudeType {
    IntegerLiteral,
    ConstantPath,
    Parenthesized
};

enum class IntegerSuffix {
    None,
    I32,
    U32,
    Isize,
    Usize
};

struct IntegerLiteralValue {
    SourceSpan span;
    std::string spelling;
    IntegerSuffix suffix = IntegerSuffix::None;
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

struct Magnitude {
    SourceSpan span;
    MagnitudeType type;
    std::optional<IntegerLiteralValue> value;
    std::optional<PathInExpression> path;
    AstPtr<Magnitude> inner;
};

enum class ConstValueType {
    Integer,
    Boolean,
    ConstantPath,
    NegatedMagnitude,
    Parenthesized
};

struct ConstValue {
    SourceSpan span;
    ConstValueType type;
    std::optional<IntegerLiteralValue> integer;
    std::optional<bool> boolean;
    std::optional<PathInExpression> path;
    AstPtr<Magnitude> magnitude;
    AstPtr<ConstValue> inner;
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

struct ExpressionStatement: Statement {
    AstPtr<Expression> expr;

    explicit ExpressionStatement(SourceSpan span): Statement(span, NodeType::ExprStmt) {}
};

struct EmptyStatement: Statement {
    explicit EmptyStatement(SourceSpan span): Statement(span, NodeType::EmptyStmt) {}
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

struct ParenthesizedType: TypeRef {
    AstPtr<TypeRef> type;

    explicit ParenthesizedType(SourceSpan span): TypeRef(span, NodeType::ParenthesizedType) {}
};
struct IntegerExpression: Expression {
    IntegerLiteralValue value;

    explicit IntegerExpression(SourceSpan span): Expression(span, NodeType::IntegerExpr) {}
};

struct BoolExpression: Expression {
    bool value = false;

    explicit BoolExpression(SourceSpan span): Expression(span, NodeType::BoolExpr) {}
};

struct UnitExpression: Expression {
    explicit UnitExpression(SourceSpan span): Expression(span, NodeType::UnitExpr) {}
};

enum class UnaryOperator {
    Negation,
    Not,
    Dereference,
    Borrow,
    BorrowMut
};

struct UnaryExpression: Expression {
    UnaryOperator op;
    AstPtr<Expression> operand;

    explicit UnaryExpression(SourceSpan span): Expression(span, NodeType::UnaryExpr) {}
};

enum class BinaryOperator {
    Add,
    Subtract,
    Multiply,
    Divide,
    Remainder,
    BitwiseAnd,
    BitwiseOr,
    BitwiseXor,
    ShiftLeft,
    ShiftRight,
    Equal,
    NotEqual,
    Greater,
    Less,
    GreaterEqual,
    LessEqual,
    LogicalAnd,
    LogicalOr
};

struct BinaryExpression: Expression {
    AstPtr<Expression> lhs_operand;
    BinaryOperator op;
    AstPtr<Expression> rhs_operand;

    explicit BinaryExpression(SourceSpan span): Expression(span, NodeType::BinaryExpr) {}
};

enum class AssignmentOperator {
    Assign,
    AssignAdd,
    AssignSubtract,
    AssignMultiply,
    AssignDivide,
    AssignRemainder,
    AssignBitwiseAnd,
    AssignBitwiseOr,
    AssignBitwiseXor,
    AssignShiftLeft,
    AssignShiftRight
};

struct AssignmentExpression: Expression {
    AstPtr<Expression> lhs_operand;
    AssignmentOperator op;
    AstPtr<Expression> rhs_operand;

    explicit AssignmentExpression(SourceSpan span): Expression(span, NodeType::AssignExpr) {}
};

struct PathExpression: Expression {
    PathInExpression path;

    explicit PathExpression(SourceSpan span): Expression(span, NodeType::PathExpr) {}
};

struct CallExpression: Expression {
    AstPtr<Expression> callee;
    std::vector<AstPtr<Expression>> args;

    explicit CallExpression(SourceSpan span): Expression(span, NodeType::CallExpr) {}
};

struct ArrayExpression: Expression {
    std::vector<AstPtr<Expression>> elements;
    std::optional<ConstValue> repeated_length;

    explicit ArrayExpression(SourceSpan span): Expression(span, NodeType::ArrayExpr) {}
};

struct IndexExpression: Expression {
    AstPtr<Expression> base;
    AstPtr<Expression> index;

    explicit IndexExpression(SourceSpan span): Expression(span, NodeType::IndexExpr) {}
};

struct FieldExpression: Expression {
    AstPtr<Expression> base;
    std::string field_name;

    explicit FieldExpression(SourceSpan span): Expression(span, NodeType::FieldExpr) {}
};

struct MethodCallExpression: Expression {
    AstPtr<Expression> receiver;
    PathExprSegment method;
    std::vector<AstPtr<Expression>> args;

    explicit MethodCallExpression(SourceSpan span): Expression(span, NodeType::MethodCallExpr) {}
};

struct CastExpression: Expression {
    AstPtr<Expression> operand;
    AstPtr<TypeRef> target_type;

    explicit CastExpression(SourceSpan span): Expression(span, NodeType::CastExpr) {}
};

struct StructExprField {
    SourceSpan span;
    std::string name;
    AstPtr<Expression> value;
};

struct StructExpression: Expression {
    PathInExpression path;
    std::vector<StructExprField> fields;

    explicit StructExpression(SourceSpan span): Expression(span, NodeType::StructExpr) {}
};

struct BlockExpression: Expression {
    std::vector<AstPtr<Statement>> statements;
    AstPtr<Expression> tail_expression;

    explicit BlockExpression(SourceSpan span): Expression(span, NodeType::BlockExpr) {}
};

struct IfExpression: Expression {
    AstPtr<Expression> condition;
    AstPtr<BlockExpression> then_block;
    AstPtr<Expression> else_branch;

    explicit IfExpression(SourceSpan span): Expression(span, NodeType::IfExpr) {}
};

struct LoopExpression: Expression {
    AstPtr<BlockExpression> body;

    explicit LoopExpression(SourceSpan span): Expression(span, NodeType::LoopExpr) {}
};

struct WhileExpression: Expression {
    AstPtr<Expression> condition;
    AstPtr<BlockExpression> body;

    explicit WhileExpression(SourceSpan span): Expression(span, NodeType::WhileExpr) {}
};

struct BreakExpression: Expression {
    AstPtr<Expression> expr;

    explicit BreakExpression(SourceSpan span): Expression(span, NodeType::BreakExpr) {}
};

struct ContinueExpression: Expression {
    explicit ContinueExpression(SourceSpan span): Expression(span, NodeType::ContinueExpr) {}
};

struct ReturnExpression: Expression {
    AstPtr<Expression> expr;

    explicit ReturnExpression(SourceSpan span): Expression(span, NodeType::ReturnExpr) {}
};

struct GroupedExpression:Expression {
    AstPtr<Expression> expr;

    explicit GroupedExpression(SourceSpan span): Expression(span, NodeType::GroupedExpr) {}
};

} // namespace ast
