#pragma once
#include "../ast/ast.hpp"
#include "diagnostic/diagnostic.hpp"
#include "semantic/type_resolver.hpp"
#include "symbol.hpp"
#include "impl_resolver.hpp"
#include "semantic_ids.hpp"
#include <string>
#include <vector>
#include <optional>

namespace semantic {

class SemanticModel;

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

const std::vector<FunctionInfo> builtin_functions = {
    FunctionInfo {
        0,
        "get_i32",
        FunctionKind::Builtin,
        nullptr,
        11,
        std::nullopt,
        std::nullopt,
    },
    FunctionInfo {
        1,
        "print_i32",
        FunctionKind::Builtin,
        nullptr,
        12,
        std::nullopt,
        std::nullopt
    },
    FunctionInfo {
        2,
        "println_i32",
        FunctionKind::Builtin,
        nullptr,
        13,
        std::nullopt,
        std::nullopt
    }
};

class FunctionResolver {
public:
    bool collectTopLevel(const ast::Crate& crate, const CrateIndex& index, SemanticModel& model, diagnostic::DiagnosticCollector& diag);
    bool resolveSignatures(const ast::Crate& crate, const CrateIndex& index, SemanticModel& model, TypeResolver& type_resolver, diagnostic::DiagnosticCollector& diag);
    bool checkEntryPoint(const ast::Crate& crate, const CrateIndex& index, SemanticModel& model, diagnostic::DiagnosticCollector& diag);

private:
    void addBuiltinFunctions(const CrateIndex& index, SemanticModel& model);
};

} // namespace semantic