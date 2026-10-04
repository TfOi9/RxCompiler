#include "place_checker.hpp"
#include "semantic/body_semantics.hpp"
#include <cassert>
#include <optional>

namespace semantic {

bool canWrite(const PlaceInfo& p) {
    return p.writable_here && !p.crossed_shared_reference && !p.blocked_by_vec_access;
}

PlaceResult PlaceChecker::fromExpression(const ast::Expression& expression, const ExprSemantics& semantics) const {
    PlaceResult result;
    result.root = &expression;
    result.type = semantics.effectiveType();
    result.category = semantics.category;
    if (semantics.category == ValueCategory::Place) {
        assert(semantics.place.has_value());
        result.access = *semantics.place;
    } else {
        result.access = PlaceInfo{};
    }
    return result;
}

PlaceResult PlaceChecker::materializeIfNeeded(PlaceResult result) const {
    if (result.category == ValueCategory::Place) {
        return result;
    }
    result.category = ValueCategory::Place;
    result.access = PlaceInfo {
        true,
        false,
        false,
    };
    result.steps.push_back(PlaceStep{
        PlaceStepKind::Materialize,
        result.type,
        std::nullopt,
        nullptr
    });
    return result;
}

std::optional<PlaceResult> PlaceChecker::dereferenceOne(PlaceResult result) const {
    const TyInfo info = types_.get(result.type);
    if (const auto* ref = std::get_if<RefTy>(&info)) {
        result.type = ref->target;
        result.category = ValueCategory::Place;
        if (!ref->is_mut) {
            result.access.crossed_shared_reference = true;
            result.access.writable_here = false;
        } else {
            result.access.writable_here = !result.access.crossed_shared_reference && !result.access.blocked_by_vec_access;
        }
        result.steps.push_back(PlaceStep{
            ref->is_mut ? PlaceStepKind::DerefMutable : PlaceStepKind::DerefShared,
            result.type,
            std::nullopt,
            nullptr
        });
        return result;
    }
    if (const auto* box = std::get_if<BoxTy>(&info)) {
        result = materializeIfNeeded(std::move(result));
        result.type = box->elem;
        result.access.writable_here = canWrite(result.access);
        result.steps.push_back(PlaceStep{
            PlaceStepKind::DerefBox,
            result.type,
            std::nullopt,
            nullptr
        });
        return result;
    }
    return std::nullopt;
}

PlaceResult PlaceChecker::projectField(PlaceResult base, TyId field_type, size_t ordinal) const {
    base = materializeIfNeeded(std::move(base));
    base.type = field_type;
    base.steps.push_back(PlaceStep{
        PlaceStepKind::Field,
        field_type,
        ordinal,
        nullptr
    });
    return base;
}

std::optional<PlaceResult> PlaceChecker::projectIndex(PlaceResult base, const ast::Expression& index) const {
    const TyInfo info = types_.get(base.type);
    TyId element_type;
    bool is_vec = false;
    if (const auto* array = std::get_if<ArrayTy>(&info)) {
        element_type = array->elem;
    } else if (const auto* vec = std::get_if<VecTy>(&info)) {
        element_type = vec->elem;
        is_vec = true;
    } else {
        return std::nullopt;
    }
    base = materializeIfNeeded(std::move(base));
    const bool base_writable = canWrite(base.access);
    if (is_vec && !base_writable) {
        base.access.blocked_by_vec_access = true;
    }
    base.access.writable_here = base_writable && !base.access.crossed_shared_reference && !base.access.blocked_by_vec_access;
    base.type = element_type;
    base.steps.push_back(PlaceStep{
        is_vec ? PlaceStepKind::VecIndex : PlaceStepKind::ArrayIndex,
        element_type,
        std::nullopt,
        &index
    });
    return base;
}

std::optional<MutableAccessError> mutableAccessError(const PlaceResult& result) {
    if (result.category != ValueCategory::Place) {
        return MutableAccessError::NotPlace;
    }
    if (result.access.crossed_shared_reference) {
        return MutableAccessError::SharedRef;
    }
    if (result.access.blocked_by_vec_access) {
        return MutableAccessError::VecAccess;
    }
    if (!result.access.writable_here) {
        return MutableAccessError::ImmutableStorage;
    }
    return std::nullopt;
}

bool requireMutableAccess(const PlaceResult& result) {
    return !mutableAccessError(result).has_value();
}

} // namespace semantic