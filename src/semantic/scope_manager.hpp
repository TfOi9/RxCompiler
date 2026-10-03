#pragma once
#include "semantic_ids.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <optional>
namespace semantic {

class BodyChecker;

struct Scope {
    std::unordered_map<std::string, LocalId> names;
};

class ScopeManager {
public:
    ScopeManager();
    void indent();
    void dedent() noexcept;
    void bind(const std::string& name, LocalId id);
    std::optional<LocalId> lookup(const std::string& name) const;
    uint32_t depth() const;
private:
    std::vector<Scope> scopes_;
};

class ScopeGuard {
public:
    ScopeGuard(ScopeManager& master): master_(master) { master_.indent(); }
    ~ScopeGuard() { master_.dedent(); }
    ScopeManager& master() { return master_; }

    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    ScopeGuard(ScopeGuard&&) = delete;
    ScopeGuard& operator=(ScopeGuard&&) = delete;

private:
    ScopeManager& master_;
};

} // namespace semantic