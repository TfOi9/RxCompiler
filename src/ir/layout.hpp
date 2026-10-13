#pragma once
#include "ir/ir_ids.hpp"
#include <vector>
#include <optional>
#include <cstdint>
#include <map>
#include <set>

namespace ir {

class Module;

struct TypeLayout {
    uint64_t size;
    uint32_t alignment;
    std::vector<uint64_t> field_offsets;
    std::optional<uint64_t> array_stride;
};

class TargetLayout {
public:
    TargetLayout(const Module& module): module_(module) {}
    TypeLayout layoutOf(TypeId type);
    void clearCache();
private:
    const Module& module_;
    std::map<TypeId, TypeLayout> cache_;
    std::set<TypeId> active_;

    TypeLayout compute(TypeId id);
    static uint64_t alignUp(uint64_t size, uint64_t alignment);
    static uint64_t checkedAdd(uint64_t lhs, uint64_t rhs);
    static uint64_t checkedMul(uint64_t lhs, uint64_t rhs);
};

} // namespace ir