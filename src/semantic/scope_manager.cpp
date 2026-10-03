#include "scope_manager.hpp"
#include <cassert>
#include <optional>

namespace semantic {

ScopeManager::ScopeManager() {
    scopes_.push_back({});
}

void ScopeManager::indent() {
    scopes_.push_back(Scope {});
}

void ScopeManager::dedent() noexcept {
    assert(scopes_.size() > 1);
    scopes_.pop_back();
}

void ScopeManager::bind(const std::string& name, LocalId id) {
    assert(!scopes_.empty());
    scopes_.back().names[name] = id;
}

std::optional<LocalId> ScopeManager::lookup(const std::string& name) const {
    if (scopes_.empty()) {
        return std::nullopt;
    }
    for (int i = scopes_.size() - 1; i >= 0; i--) {
        if (scopes_.at(i).names.count(name)) {
            return scopes_.at(i).names.at(name);
        }
    }
    return std::nullopt;
}

uint32_t ScopeManager::depth() const {
    assert(!scopes_.empty());
    return scopes_.size() - 1;
}

} // namespace semantic