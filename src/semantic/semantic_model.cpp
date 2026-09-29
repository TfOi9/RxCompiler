#include "semantic_model.hpp"
#include "struct_resolver.hpp"
#include "semantic/symbol.hpp"

namespace semantic {

StructInfo* SemanticModel::findStruct(SymbolId id) {
    return structs_.count(id) ? &structs_[id] : nullptr;
}

} // namespace semantic
