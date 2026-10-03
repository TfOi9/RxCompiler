#include "body_semantics.hpp"
#include "semantic/types.hpp"
#include "semantic_model.hpp"

namespace semantic {

TyId ExprSemantics::effectiveType() const {
    return coerced_type.value_or(type);
}

TyId effectiveType(const ExprSemantics& info) {
    return info.coerced_type.value_or(info.type);
}

bool isInteger(TyId type, SemanticModel &model) {
    return type == model.typeContext().insert(PrimaryTy{PrimaryTyKind::I32})
        || type == model.typeContext().insert(PrimaryTy{PrimaryTyKind::U32})
        || type == model.typeContext().insert(PrimaryTy{PrimaryTyKind::ISize})
        || type == model.typeContext().insert(PrimaryTy{PrimaryTyKind::USize});
}

bool isSignedInteger(TyId type, SemanticModel &model) {
    return type == model.typeContext().insert(PrimaryTy{PrimaryTyKind::I32})
        || type == model.typeContext().insert(PrimaryTy{PrimaryTyKind::ISize});
}

bool isBool(TyId type, SemanticModel &model) {
    return type == model.typeContext().insert(PrimaryTy{PrimaryTyKind::Bool});
}

bool isNever(TyId type, SemanticModel &model) {
    return type == model.typeContext().insert(NeverTy{});
}

bool isError(TyId type, SemanticModel &model) {
    return type == model.typeContext().insert(ErrorTy{});
}

} // namespace semantic