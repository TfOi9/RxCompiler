#pragma once
#include "ast/ast.hpp"
#include "body_semantics.hpp"
#include "semantic/types.hpp"
#include <optional>
#include <vector>

namespace semantic {

enum class PlaceStepKind {
    Materialize,
    DerefShared,
    DerefMutable,
    DerefBox,
    Field,
    ArrayIndex,
    VecIndex
};

struct PlaceStep {
    PlaceStepKind kind;
    TyId result_type;
    std::optional<size_t> field_ordinal;
    const ast::Expression* index_expression = nullptr;
};

struct PlaceResult {
    const ast::Expression* root = nullptr;
    TyId type;
    ValueCategory category = ValueCategory::Value;
    PlaceInfo access;
    std::vector<PlaceStep> steps;
};

bool canWrite(const PlaceInfo& p);

class PlaceChecker {
public:
    explicit PlaceChecker(const TypeContext& types): types_(types) {}
    PlaceResult fromExpression(const ast::Expression& expression, const ExprSemantics& semantics) const;
    PlaceResult materializeIfNeeded(PlaceResult result) const;
    std::optional<PlaceResult> dereferenceOne(PlaceResult result) const;
    PlaceResult projectField(PlaceResult base, TyId field_type, size_t ordinal) const;
    std::optional<PlaceResult> projectIndex(PlaceResult base, const ast::Expression& index) const;
private:
    const TypeContext& types_;
};

enum class MutableAccessError {
    NotPlace,
    SharedRef,
    VecAccess,
    ImmutableStorage
};

std::optional<MutableAccessError> mutableAccessError(const PlaceResult& result);
bool requireMutableAccess(const PlaceResult& result);

} // namespace semantic