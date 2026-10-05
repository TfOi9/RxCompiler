#pragma once
#include "ast/ast.hpp"
#include "body_semantics.hpp"
#include "semantic/place_checker.hpp"
#include "semantic/types.hpp"
#include <optional>
#include <vector>

namespace semantic {

struct ResultSite {
    const ast::Expression* expression;
    ExprSemantics original;
};

struct LubPlan {
    TyId target_type;
    std::vector<CoercionPlan> coversions;
};

class CoercionChecker {
public:
    explicit CoercionChecker(const TypeContext& types): types_(types), places_(types) {}
    std::optional<CoercionPlan> tryCoerce(const ast::Expression& expr, const ExprSemantics& original, TyId target) const;
    std::optional<LubPlan> tryFindCommonType(const std::vector<ResultSite>& sites, TyId never_type);
    std::optional<std::vector<CoercionPlan>> planAll(const std::vector<ResultSite>& sites, size_t count, TyId target);
private:
    const TypeContext& types_;
    PlaceChecker places_;
};

} // namespace semantic