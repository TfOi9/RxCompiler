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

struct TypeRef;

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

struct GenericParam;

struct WhereClause {
    // TODO unimplemented
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

} // namespace ast