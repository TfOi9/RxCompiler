#include "types.hpp"
#include <stdexcept>

namespace semantic {

TyId TypeContext::insert(const TyInfo& node) {
    if (ids_.count(node)) {
        return ids_[node];
    } else {
        types_.push_back(node);
        ids_[node] = types_.size() - 1;
        return types_.size() - 1;
    }
}

const TyInfo& TypeContext::get(TyId id) const {
    if (id >= types_.size()) {
        throw std::runtime_error("invalid typeid");
    }
    return types_[id];
}

bool TypeContext::equal(const TyInfo& a, const TyInfo& b) const {
    if (!ids_.count(a) || !ids_.count(b)) return false;
    return ids_.at(a) == ids_.at(b);
}

} // namespace semantic