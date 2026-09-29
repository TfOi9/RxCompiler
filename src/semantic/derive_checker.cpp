#include "derive_checker.hpp"
#include "semantic_model.hpp"
#include "struct_resolver.hpp"

namespace semantic {

std::string DeriveChecker::kindName(DeriveKind trait) {
    switch (trait) {
        case DeriveKind::Copy: return "Copy";
        case DeriveKind::Clone: return "Clone";
        case DeriveKind::PartialEq: return "PartialEq";
        case DeriveKind::Eq: return "Eq";
    }
}

bool DeriveChecker::supportWith(TyId type, DeriveKind trait) const {
    const auto& info = model_.types_.get(type);
    if (std::holds_alternative<ErrorTy>(info)) {
        return true;
    }
    if (std::holds_alternative<UnitTy>(info)) {
        return true;
    }
    if (std::holds_alternative<PrimaryTy>(info)) {
        return true;
    }
    if (std::holds_alternative<NeverTy>(info)) {
        return true;
    }
    if (const auto* ref = std::get_if<RefTy>(&info)) {
        if (trait == DeriveKind::Copy || trait == DeriveKind::Clone) {
            return !ref->is_mut;
        } else {
            return supportWith(ref->target, trait);
        }
    }
    if (const auto* arr = std::get_if<ArrayTy>(&info)) {
        return supportWith(arr->elem, trait);
    }
    if (const auto* box = std::get_if<BoxTy>(&info)) {
        if (trait == DeriveKind::Copy) {
            return false;
        } else {
            return supportWith(box->elem, trait);
        }
    }
    if (const auto* vec = std::get_if<VecTy>(&info)) {
        if (trait == DeriveKind::Copy) {
            return false;
        } else {
            return supportWith(vec->elem, trait);
        }
    }
    if (const auto* stru = std::get_if<StructTy>(&info)) {
        auto it = model_.structs_.find(stru->def);
        return it != model_.structs_.end() && it->second.derives.valid.has(trait);
    }
    return false;
}

bool DeriveChecker::structRequirementsHold(const StructInfo& info, DeriveKind trait) const {
    if (trait == DeriveKind::Copy && !info.derives.requested.has(DeriveKind::Clone)) {
        return false;
    }
    if (trait == DeriveKind::Eq && !info.derives.requested.has(DeriveKind::PartialEq)) {
        return false;
    }
    for (const auto& field: info.fields) {
        if (!supportWith(field.type, trait)) {
            return false;
        }
    }
    return true;
}

void DeriveChecker::computeValid() {
    for (auto& [id, info]: model_.structs_) {
        info.derives.valid = info.derives.requested;
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& [id, info]: model_.structs_) {
            for (DeriveKind k: {DeriveKind::Copy, DeriveKind::Clone, DeriveKind::PartialEq, DeriveKind::Eq}) {
                if (info.derives.valid.has(k) && !structRequirementsHold(info, k)) {
                    info.derives.valid.set(k, false);
                    changed = true;
                }
            }
        }
    }
    computed_ = true;
}

bool DeriveChecker::reportStructErrors() {
    bool ok = true;
    for (auto& [id, info]: model_.structs_) {
        for (DeriveKind k: {DeriveKind::Copy, DeriveKind::Clone, DeriveKind::PartialEq, DeriveKind::Eq}) {
            if (info.derives.valid.has(k) || !info.derives.requested.has(k)) continue;
            if (k == DeriveKind::Copy && !info.derives.requested.has(DeriveKind::Clone)) {
                diag_.add_entry(diagnostic::Severity::Error, info.declaration->span.begin, "missing Clone trait for struct " + info.name);
            } else if (k == DeriveKind::Eq && !info.derives.requested.has(DeriveKind::PartialEq)) {
                diag_.add_entry(diagnostic::Severity::Error, info.declaration->span.begin, "missing PartialEq trait for struct " + info.name);
            } else {
                for (const auto& field: info.fields) {
                    if (!supportWith(field.type, k)) {
                        diag_.add_entry(diagnostic::Severity::Error, field.declaration->span.begin, "field " + field.name + " of struct " + info.name + " is missing trait " + kindName(k));
                        break;
                    }
                }
            }
            ok = false;
        }
    }
    reported_ = true;
    return ok;
}

bool DeriveChecker::checkAll() {
    computeValid();
    return reportStructErrors();
}

bool DeriveChecker::supports(TyId type, DeriveKind trait) {
    if (!computed_) {
        computeValid();
    }
    if (!reported_) {
        reportStructErrors();
    }
    return supportWith(type, trait);
}

} // namespace semantic