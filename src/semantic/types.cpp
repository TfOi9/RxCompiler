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

TyId TypeContext::error() {
    return insert(ErrorTy {});
}

const TyInfo& TypeContext::get(TyId id) const {
    if (id >= types_.size()) {
        throw std::runtime_error("invalid typeid");
    }
    return types_[id];
}

bool TypeContext::equal(const TyInfo& a, const TyInfo& b) const {
    return !(a < b) && !(b < a);
}

} // namespace semantic
