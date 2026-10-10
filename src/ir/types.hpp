#pragma once
#include "ir/ir_ids.hpp"
#include <cstdint>
#include <variant>
#include <vector>
#include <string>
#include <optional>
#include <map>

namespace ir {

struct VoidType {
    bool operator<(const VoidType&) const {
        return false;
    }
};
struct IntegerType {
    uint8_t bits;
    bool operator<(const IntegerType& other) const {
        return bits < other.bits;
    }
};
struct PointerType {
    bool operator<(const PointerType&) const {
        return false;
    }
};
struct ArrayType {
    TypeId element;
    uint32_t length;
    bool operator<(const ArrayType& other) const {
        if (element == other.element) {
            return length < other.length;
        }
        return element < other.element;
    }
};
struct NamedStructType {
    StructId id;
    bool operator<(const NamedStructType& other) const {
        return id < other.id;
    }
};

struct StructDef {
    std::string name;
    std::optional<std::vector<TypeId>> fields;
};

using Type = std::variant<VoidType, IntegerType, PointerType, ArrayType, NamedStructType>;

class TypeTable {
public:
    TypeTable();
    TypeId insert(const Type& type);
    std::optional<TypeId> find(const Type& type) const;
    Type get(TypeId id) const;
    bool contains(TypeId id) const;

    TypeId none() const;
    TypeId i1() const;
    TypeId i8() const;
    TypeId i32() const;
    TypeId ptr() const;

    uint8_t integerSize(TypeId id) const;

    TypeId arrayOf(TypeId id, uint32_t length);
    TypeId structOf(StructId id);

    bool isInteger(TypeId id) const;
    bool isPointer(TypeId id) const;

private:
    std::vector<Type> types_;
    std::map<Type, TypeId> type_map_;
};

} // namespace ir
