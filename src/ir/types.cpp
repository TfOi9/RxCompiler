#include "ir/types.hpp"
#include <optional>
#include <cassert>

namespace ir {

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

} // namespace ir