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
    assert(!scopes_.empty());
    scopes_.pop_back();
}

void ScopeManager::bind(const std::string& name, LocalId id) {
    scopes_.back().names[name] = id;
}

std::optional<LocalId> ScopeManager::lookup(const std::string& name) const {
    if (scopes_.empty()) {
        return std::nullopt;
    }
    if (!scopes_.back().names.count(name)) {
        return std::nullopt;
    }
    return scopes_.back().names.at(name);
}

uint32_t ScopeManager::depth() const {
    return scopes_.size() - 1;
}

} // namespace semantic