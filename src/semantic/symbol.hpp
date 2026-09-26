#pragma once
#include "../ast/ast.hpp"
#include "../diagnostic/diagnostic.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>

namespace semantic {

using SymbolId = uint32_t;

enum class SymbolKind {
    Struct,
    Function,
    Constant,
    BuiltinType,
    BuiltinValue
};

struct Symbol {
    SymbolId id;
    SymbolKind kind;
    std::string name;
    const ast::Item* declaration = nullptr;
};

const std::vector<Symbol> builtins = {
    Symbol {
        0,
        SymbolKind::BuiltinType,
        "i32",
    },
    Symbol {
        1,
        SymbolKind::BuiltinType,
        "u32",
    },
    Symbol {
        2,
        SymbolKind::BuiltinType,
        "isize",
    },
    Symbol {
        3,
        SymbolKind::BuiltinType,
        "usize",
    },
    Symbol {
        4,
        SymbolKind::BuiltinType,
        "bool",
    },
    Symbol {
        5,
        SymbolKind::BuiltinType,
        "Box",
    },
    Symbol {
        6,
        SymbolKind::BuiltinType,
        "Vec",
    },
    Symbol {
        7,
        SymbolKind::BuiltinType,
        "Copy",
    },
    Symbol {
        8,
        SymbolKind::BuiltinType,
        "Clone",
    },
    Symbol {
        9,
        SymbolKind::BuiltinType,
        "PartialEq",
    },
    Symbol {
        10,
        SymbolKind::BuiltinType,
        "Eq",
    },
    Symbol {
        11,
        SymbolKind::BuiltinValue,
        "get_i32",
    },
    Symbol {
        12,
        SymbolKind::BuiltinValue,
        "print_i32",
    },
    Symbol {
        13,
        SymbolKind::BuiltinValue,
        "println_i32",
    },
};

struct CrateIndex {
    std::vector<Symbol> symbols;
    std::unordered_map<std::string, SymbolId> type_names;
    std::unordered_map<std::string, SymbolId> value_names;
    std::vector<const ast::ImplItem*> pending_impls;
    std::unordered_map<const ast::Item*, SymbolId> item_symbols;

    CrateIndex();
    SymbolId nextId() const;
    void addSymbol(const Symbol& sym, const ast::Item* declaration, diagnostic::DiagnosticCollector* diagnostics);
    void addImpl(const ast::ImplItem* impl);
    void addBuiltinSymbols();
    void dump(std::ostream& os) const;
};

CrateIndex collectDeclarations(const ast::Crate* crate, diagnostic::DiagnosticCollector* diagnostics);

} // namespace semantic