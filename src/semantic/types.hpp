#pragma once
#include "../ast/ast.hpp"
#include "symbol.hpp"
#include <cstdint>
#include <variant>
#include <vector>
#include <map>

namespace semantic {

using TyId = uint32_t;

enum class PrimaryTyKind {
    Bool, I32, U32, ISize, USize
};

struct PrimaryTy {
    PrimaryTyKind ty;
};

struct StructTy {
    SymbolId def;
};

struct RefTy {
    TyId target;
    bool is_mut = false;
};

struct ArrayTy {
    TyId elem;
    uint32_t len;
};

struct BoxTy {
    TyId elem;
};

struct VecTy {
    TyId elem;
};

struct ErrorTy {};
struct UnitTy {};
struct NeverTy {};

using TyInfo = std::variant<
    PrimaryTy, StructTy, RefTy, ArrayTy,
    BoxTy, VecTy, ErrorTy, UnitTy, NeverTy
>;

class TypeContext {
public:
    TyId insert(const TyInfo& node);
    const TyInfo& get(TyId id) const;
    bool equal(const TyInfo& a, const TyInfo& b) const;

private:
    std::vector<TyInfo> types_;
    std::map<TyInfo, TyId> ids_;
};

} // namespace semantic