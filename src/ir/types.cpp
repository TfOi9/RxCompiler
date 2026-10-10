#include "ir/types.hpp"
#include "ir/ir_ids.hpp"
#include <optional>
#include <cassert>

namespace ir {

TypeTable::TypeTable() {
    insert(VoidType{});
    insert(IntegerType{1});
    insert(IntegerType{8});
    insert(IntegerType{32});
    insert(PointerType{});
}

TypeId TypeTable::insert(const Type& type) {
    if (!type_map_.count(type)) {
        TypeId id = types_.size();
        types_.push_back(type);
        type_map_[type] = id;
        return id;
    }
    return type_map_.at(type);
}

std::optional<TypeId> TypeTable::find(const Type& type) const {
    if (!type_map_.count(type)) {
        return std::nullopt;
    }
    return type_map_.at(type);
}

Type TypeTable::get(TypeId id) const {
    assert(id < types_.size());
    return types_.at(id);
}

bool TypeTable::contains(TypeId id) const {
    return id < types_.size();
} 

TypeId TypeTable::none() const {
    return *find(VoidType{});
}

TypeId TypeTable::i1() const {
    return *find(IntegerType{1});
}

TypeId TypeTable::i8() const {
    return *find(IntegerType{8});
}

TypeId TypeTable::i32() const {
    return *find(IntegerType{32});
}

TypeId TypeTable::ptr() const {
    return *find(PointerType{});
}

uint8_t TypeTable::integerSize(TypeId id) const {
    assert(isInteger(id));
    if (id == i1()) {
        return 1;
    } else if (id == i8()) {
        return 8;
    } else if (id == i32()) {
        return 32;
    }
    return 0;
}

TypeId TypeTable::arrayOf(TypeId id, uint32_t length) {
    return insert(ArrayType{id, length});
}

TypeId TypeTable::structOf(StructId id) {
    return insert(NamedStructType{id});
}

bool TypeTable::isInteger(TypeId id) const {
    return id == i1() || id == i8() || id == i32();
}

bool TypeTable::isPointer(TypeId id) const {
    return id == ptr();
}

} // namespace ir