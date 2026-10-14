#pragma once
#include "ir/builder.hpp"
#include "ir/function.hpp"
#include "ir/ir_ids.hpp"
#include "semantic/semantic_ids.hpp"
#include "semantic/semantic_model.hpp"
#include <optional>
#include <map>

namespace ir {

enum class LoweredKind {
    Scalar,
    Aggregate,
    Unit,
    Never
};

struct LoweredType {
    std::optional<TypeId> storage_type;
    std::optional<TypeId> value_type;
};

class TypeLowering {
public:
    TypeLowering(const semantic::SemanticModel& model, Module& module): model_(model), module_(module), builder_(module_) {}
    LoweredType lower(semantic::TyId type);

    TypeId storageType(semantic::TyId type);
    TypeId valueType(semantic::TyId type);
    TypeId returnType(semantic::TyId type);

    TypeId declareStruct(semantic::SymbolId definition);
    TypeId defineStruct(semantic::SymbolId definition);

private:
    enum class StructState {
        Declared,
        Defining,
        Defined
    };

    struct StructEntry {
        StructId id;
        TypeId type;
        StructState state;
    };

    const semantic::SemanticModel& model_;
    Module& module_;
    IrBuilder builder_;
    std::map<semantic::TyId, LoweredType> type_cache_;
    std::map<semantic::SymbolId, StructEntry> struct_cache_;
    std::optional<TypeId> vec_type_;
    
    LoweredType lowerUncached(semantic::TyId type);
    LoweredType lowerStruct(semantic::SymbolId definition);
    TypeId vectorType();
};

} // namespace ir