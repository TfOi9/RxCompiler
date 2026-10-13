#include "type_lowering.hpp"
#include "ir/builder.hpp"
#include "ir/ir_ids.hpp"
#include "semantic/semantic_ids.hpp"
#include "semantic/types.hpp"
#include <optional>
#include <string>
#include <variant>

namespace ir {

LoweredType TypeLowering::lower(semantic::TyId type) {
    if (auto it = type_cache_.find(type); it != type_cache_.end()) {
        return it->second;
    }
    LoweredType result = lowerUncached(type);
    type_cache_.emplace(type, result);
    return result;
}

TypeId TypeLowering::storageType(semantic::TyId type) {
    auto result = lower(type);
    IrBuilder::requireIr(result.storage_type.has_value(), "semantic type has no IR storage type");
    return *result.storage_type;
}

TypeId TypeLowering::valueType(semantic::TyId type) {
    auto result = lower(type);
    IrBuilder::requireIr(result.value_type.has_value(), "semantic type has no IR value type");
    return *result.value_type;
}

TypeId TypeLowering::returnType(semantic::TyId type) {
    const auto& info = model_.typeContext().get(type);
    if (std::holds_alternative<semantic::UnitTy>(info)) {
        return module_.types.none();
    }
    return valueType(type);
}

LoweredType TypeLowering::lowerUncached(semantic::TyId type) {
    const auto& info = model_.typeContext().get(type);
    if (const auto* primary = std::get_if<semantic::PrimaryTy>(&info)) {
        switch (primary->ty) {
            case semantic::PrimaryTyKind::I32:
            case semantic::PrimaryTyKind::U32:
            case semantic::PrimaryTyKind::ISize:
            case semantic::PrimaryTyKind::USize:
                return {
                    module_.types.i32(),
                    module_.types.i32()
                };
            case semantic::PrimaryTyKind::Bool:
                return {
                    module_.types.i8(),
                    module_.types.i1()
                };
        }
    } else if (std::holds_alternative<semantic::RefTy>(info) || std::holds_alternative<semantic::BoxTy>(info)) {
        return {
            module_.types.ptr(),
            module_.types.ptr()
        };
    } else if (const auto* arr = std::get_if<semantic::ArrayTy>(&info)) {
        TypeId elem = storageType(arr->elem);
        TypeId result = module_.types.arrayOf(elem, arr->len);
        return {result, result};
    } else if (std::holds_alternative<semantic::UnitTy>(info) || std::holds_alternative<semantic::NeverTy>(info)) {
        return {
            std::nullopt,
            std::nullopt
        };
    } else if (std::holds_alternative<semantic::ErrorTy>(info)) {
        throw std::logic_error("semantic error type cannot be lowered to IR type");
    }
    throw std::logic_error("unexpected semantic type");
}

LoweredType TypeLowering::lowerStruct(semantic::SymbolId definition) {
    auto it = struct_cache_.find(definition);
    if (it != struct_cache_.end() && it->second.state == StructState::Defined) {
        return {it->second.type, it->second.type};
    }
    const auto* info = model_.findStruct(definition);
    IrBuilder::requireIr(info != nullptr, "unknown semantic struct");
    if (it == struct_cache_.end()) {
        StructId id = builder_.declareStruct("__rx_struct_" + std::to_string(definition));
        TypeId type = module_.types.structOf(id);
        it = struct_cache_.emplace(definition, StructEntry{id, type, StructState::Declared}).first;
    }
    StructEntry& entry = it->second;
    IrBuilder::requireIr(entry.state == StructState::Defined, "recursive by-value semantic struct");
    entry.state = StructState::Defining;
    try {
        std::vector<TypeId> fields;
        fields.reserve(info->fields.size());
        for (const auto& field: info->fields) {
            fields.push_back(storageType(field.type));
        }
        builder_.defineStruct(entry.id, std::move(fields));
        entry.state = StructState::Defined;
    } catch (...) {
        entry.state = StructState::Declared;
        throw;
    }
    return {entry.type, entry.type};
}

TypeId TypeLowering::vectorType() {
    if (vec_type_.has_value()) {
        return *vec_type_;
    }
    StructId id = builder_.declareStruct("__rx_vec");
    builder_.defineStruct(id, {module_.types.ptr(), module_.types.i32(), module_.types.i32()});
    TypeId type = module_.types.structOf(id);
    vec_type_ = type;
    return type;
}

} // namespace ir