#pragma once
#include "../ast/ast.hpp"
#include "diagnostic/diagnostic.hpp"
#include "semantic/type_resolver.hpp"
#include "symbol.hpp"
#include "impl_resolver.hpp"
#include "types.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace semantic {

class SemanticModel;

using FunctionId = uint32_t;

enum class FunctionKind {
    TopLevel,
    Associated,
    Builtin
};

enum class ReceiverMode {
    None,
    ByValue,
    Ref,
    MutableRef
};

struct ParameterInfo {
    std::string name;
    TyId type;
    bool is_mut = false;
    bool is_receiver = false;
    ast::SourceSpan span;
};

struct FunctionSignature {
    std::vector<ParameterInfo> parameters;
    TyId return_type;
    ReceiverMode receiver_mode = ReceiverMode::None;
};

struct FunctionInfo {
    FunctionId id;
    std::string name;
    FunctionKind kind;
    const ast::FunctionItem* declaration = nullptr;
    std::optional<SymbolId> top_level_symbol;
    std::optional<AssocId> associated_item;
    std::optional<SymbolId> owner_struct;
    FunctionSignature signature;
    bool signature_valid = false;
};

class FunctionResolver {
public:
    bool collectTopLevel(const ast::Crate& crate, const CrateIndex& index, SemanticModel& model, diagnostic::DiagnosticCollector& diag);
    bool resolveSignatures(const ast::Crate& crate, const CrateIndex& index, SemanticModel& model, TypeResolver& type_resolver, diagnostic::DiagnosticCollector& diag);
};

} // namespace semantic