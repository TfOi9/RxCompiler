#pragma once
#include "semantic_ids.hpp"
#include <cstdint>
#include <variant>
#include <vector>
#include <map>

namespace semantic {

enum class PrimaryTyKind {
    Bool, I32, U32, ISize, USize
};

struct PrimaryTy {
    PrimaryTyKind ty;

    bool operator<(const PrimaryTy& other) const {
        return ty < other.ty;
    }
};

struct StructTy {
    SymbolId def;

    bool operator<(const StructTy& other) const {
        return def < other.def;
    }
};

struct RefTy {
    TyId target;
    bool is_mut = false;

    bool operator<(const RefTy& other) const {
        if (target != other.target) return target < other.target;
        return is_mut < other.is_mut;
    }
};

struct ArrayTy {
    TyId elem;
    uint32_t len;

    bool operator<(const ArrayTy& other) const {
        if (elem != other.elem) return elem < other.elem;
        return len < other.len;
    }
};

struct BoxTy {
    TyId elem;

    bool operator<(const BoxTy& other) const {
        return elem < other.elem;
    }
};

struct VecTy {
    TyId elem;

    bool operator<(const VecTy& other) const {
        return elem < other.elem;
    }
};

struct ErrorTy {
    bool operator<(const ErrorTy&) const { return false; }
};
struct UnitTy {
    bool operator<(const UnitTy&) const { return false; }
};
struct NeverTy {
    bool operator<(const NeverTy&) const { return false; }
};

using TyInfo = std::variant<
    PrimaryTy, StructTy, RefTy, ArrayTy,
    BoxTy, VecTy, ErrorTy, UnitTy, NeverTy
>;

class TypeContext {
public:
    TyId insert(const TyInfo& node);
    TyId error();
    const TyInfo& get(TyId id) const;
    bool equal(const TyInfo& a, const TyInfo& b) const;

private:
    std::vector<TyInfo> types_;
    std::map<TyInfo, TyId> ids_;
};

} // namespace semantic
