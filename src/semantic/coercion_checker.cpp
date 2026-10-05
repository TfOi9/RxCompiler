#include "coercion_checker.hpp"
#include "semantic/body_semantics.hpp"
#include "semantic/types.hpp"
#include <cassert>
#include <optional>
#include <variant>

namespace semantic {

std::optional<CoercionPlan> CoercionChecker::tryCoerce(const ast::Expression& expr, const ExprSemantics& original, TyId target) const {
    assert(!original.coerced_type.has_value());
    const TyId source = original.type;
    const TyInfo source_info = types_.get(source);
    const TyInfo target_info = types_.get(target);
    if (std::holds_alternative<ErrorTy>(source_info) || std::holds_alternative<ErrorTy>(target_info)) {
        return CoercionPlan{
            source,
            target,
            CoercionKind::Recovery,
            {},
            std::nullopt
        };
    }
    if (std::holds_alternative<NeverTy>(source_info)) {
        return CoercionPlan{
            source,
            target,
            CoercionKind::NeverToAny,
            {},
            std::nullopt
        };
    }
    const auto* source_ref = std::get_if<RefTy>(&source_info);
    const auto* target_ref = std::get_if<RefTy>(&target_info);
    const bool mutable_reborrow = source == target && source_ref != nullptr && source_ref->is_mut;
    if (source == target && !mutable_reborrow) {
        return CoercionPlan{
            source,
            target,
            CoercionKind::Identity,
            {},
            std::nullopt
        };
    }
    if (!source_ref || !target_ref) {
        return std::nullopt;
    }
    if (target_ref->is_mut && !source_ref->is_mut) {
        return std::nullopt;
    }
    PlaceResult cursor = places_.fromExpression(expr, original);
    auto first = places_.dereferenceOne(std::move(cursor));
    if (!first) {
        return std::nullopt;
    }
    cursor = std::move(*first);
    while (cursor.type != target_ref->target) {
        auto next = places_.dereferenceOne(std::move(cursor));
        if (!next) {
            return std::nullopt;
        }
        cursor = std::move(*next);
    }
    return CoercionPlan{
        source,
        target,
        target_ref->is_mut ? CoercionKind::BorrowMutable : CoercionKind::BorrowShared,
        std::move(cursor.steps),
        cursor.access
    };
}

std::optional<LubPlan> CoercionChecker::tryFindCommonType(const std::vector<ResultSite>& sites, TyId never_type) {
    if (sites.empty()) {
        return std::nullopt;
    }
    for (const auto& site: sites) {
        assert(site.expression != nullptr);
        assert(!site.original.coerced_type.has_value());
    }
    for (const auto& site: sites) {
        const TyId type = site.original.type;
        if (std::holds_alternative<ErrorTy>(types_.get(type))) {
            auto plans = planAll(sites, sites.size(), type);
            assert(plans.has_value());
            return LubPlan{
                type,
                std::move(*plans)
            };
        }
    }
    std::optional<TyId> target;
    for (size_t i = 0; i < sites.size(); i++) {
        const auto& site = sites[i];
        const TyId next_type = site.original.type;
        if (std::holds_alternative<NeverTy>(types_.get(next_type))) {
            continue;
        }
        if (!target) {
            target = next_type;
            continue;
        }
        if (tryCoerce(*site.expression, site.original, *target)) {
            continue;
        }
        auto replacement = planAll(sites, i + 1, next_type);
        if (!replacement) {
            return std::nullopt;
        }
        target = next_type;
    }
    const TyId final_type = target.value_or(never_type);
    auto final_plans = planAll(sites, sites.size(), final_type);
    assert(final_plans.has_value());
    return LubPlan{
        final_type,
        std::move(*final_plans)
    };
}

std::optional<std::vector<CoercionPlan>> CoercionChecker::planAll(const std::vector<ResultSite>& sites, size_t count, TyId target) {
    std::vector<CoercionPlan> plans;
    plans.reserve(count);
    for (size_t i = 0; i < count; i++) {
        const auto& site = sites[i];
        auto plan = tryCoerce(*site.expression, site.original, target);
        if (!plan) {
            return std::nullopt;
        }
        plans.push_back(std::move(*plan));
    }
    return plans;
}

} // namespace semantic