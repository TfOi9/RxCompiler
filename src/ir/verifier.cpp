#include "ir/verifier.hpp"
#include "ir/ir_ids.hpp"
#include "ir/types.hpp"
#include <optional>
#include <unordered_map>
#include <functional>

namespace ir {

std::vector<IrError> IrVerifier::checkStructLoop() const {
    enum class StructStatus {
        Unknown,
        Visiting,
        Visited
    };
    std::unordered_map<StructId, StructStatus> status;
    std::function<bool(StructId)> visit = [&](StructId pos) -> bool {
        if (!status.count(pos)) {
            status[pos] = StructStatus::Visiting;
        } else return true;
        const auto& def = module_.structs.at(pos);
        if (!def.fields) {
            return true;
        }
        bool ok = true;
        for (const auto& field: *def.fields) {
            const auto& type = module_.types.get(field);
            const auto* named = std::get_if<NamedStructType>(&type);
            if (!named) {
                continue;
            }
            const StructId id = named->id;
            if (status.count(id)) {
                if (status.at(id) == StructStatus::Visiting) {
                    return false;
                }
            } else {
                ok = visit(id) && ok;
            }
        }
        if (!ok) {
            return false;
        }
        status[pos] = StructStatus::Visited;
        return true;
    };
    std::vector<IrError> errors;
    for (size_t i = 0; i < module_.structs.size(); i++) {
        if (status.count(i) && status.at(i) == StructStatus::Visited) {
            continue;
        }
        if ((status.count(i) && status.at(i) == StructStatus::Visiting) || (!status.count(i) && !visit(i))) {
            errors.push_back(
                IrError{
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    i,
                    "struct " + module_.structs.at(i).name + " has infinite layout"
                }
            );
        }
    }
    return errors;
}

} // namespace ir