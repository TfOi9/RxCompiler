#include "ir/layout.hpp"
#include "ir/ir_ids.hpp"
#include "ir/function.hpp"
#include "ir/types.hpp"
#include <optional>
#include <stdexcept>
#include <limits>
#include <variant>

namespace ir {

TypeLayout TargetLayout::layoutOf(TypeId type) {
    if (auto it = cache_.find(type); it != cache_.end()) {
        return it->second;
    }
    if (active_.count(type)) {
        throw std::logic_error("recursive IR object layout");
    }
    try {
        TypeLayout layout = compute(type);
        cache_.emplace(type, layout);
        active_.erase(type);
        return layout;
    } catch (...) {
        active_.erase(type);
        throw;
    }
}

void TargetLayout::clearCache() {
    cache_.clear();
}

uint64_t TargetLayout::alignUp(uint64_t size, uint64_t alignment) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        throw std::invalid_argument("invalide type alignment");
    }
    uint64_t rem = size % alignment;
    if (rem == 0) {
        return size;
    }
    uint64_t padding = uint64_t(alignment) - rem;
    return checkedAdd(size, padding);
}

uint64_t TargetLayout::checkedAdd(uint64_t lhs, uint64_t rhs) {
    constexpr uint64_t max = std::numeric_limits<uint64_t>::max();
    if (rhs > max - lhs) {
        throw std::overflow_error("type layout addition overflow");
    }
    return lhs + rhs;
}

uint64_t TargetLayout::checkedMul(uint64_t lhs, uint64_t rhs) {
    constexpr uint64_t max = std::numeric_limits<uint64_t>::max();
    if (lhs != 0 && rhs > max / lhs) {
        throw std::overflow_error("type layout multiplication overflow");
    }
    return lhs * rhs;
}

TypeLayout TargetLayout::compute(TypeId id) {
    Type type = module_.types.get(id);
    if (std::holds_alternative<VoidType>(type)) {
        throw std::logic_error("void has no object layout");
    } else if (const auto* integer = std::get_if<IntegerType>(&type)) {
        switch (integer->bits) {
            case 1:
            case 8:
                return TypeLayout{
                    1,
                    1,
                    {},
                    std::nullopt
                };
            case 32:
                return TypeLayout{
                    4,
                    4,
                    {},
                    std::nullopt
                };
            default:
                throw std::logic_error("unsupported integer layout");
        }
    } else if (std::holds_alternative<PointerType>(type)) {
        return TypeLayout{
            4,
            4,
            {},
            std::nullopt
        };
    } else if (const auto* arr = std::get_if<ArrayType>(&type)) {
        TypeLayout elem = layoutOf(arr->element);
        uint64_t stride = alignUp(elem.size, elem.alignment);
        uint64_t size = checkedMul(stride, arr->length);
        return TypeLayout{
            size,
            elem.alignment,
            {},
            stride
        };
    } else if (const auto* named = std::get_if<NamedStructType>(&type)) {
        const StructDef& def = module_.structs.at(named->id);
        if (!def.fields.has_value()) {
            throw std::logic_error("opaque struct has no object layout");
        }
        TypeLayout result{
            0,
            1,
            {},
            std::nullopt
        };
        const auto& fields = *def.fields;
        result.field_offsets.reserve(fields.size());
        uint64_t cursor = 0;
        for (auto field_type: fields) {
            TypeLayout layout = layoutOf(field_type);
            cursor = alignUp(cursor, layout.alignment);
            result.field_offsets.push_back(cursor);
            cursor = checkedAdd(cursor, layout.size);
            result.alignment = std::max(result.alignment, layout.alignment);
        }
        result.size = alignUp(cursor, result.alignment);
        return result;
    }
    throw std::logic_error("unsupported IR type layout");
}

} // namespace ir