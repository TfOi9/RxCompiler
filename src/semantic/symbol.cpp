#include "symbol.hpp"
#include "ast/ast.hpp"
#include "diagnostic/diagnostic.hpp"

namespace semantic {

CrateIndex::CrateIndex() {
    addBuiltinSymbols();
}

SymbolId CrateIndex::nextId() const {
    return static_cast<SymbolId>(symbols.size());
}

void CrateIndex::addSymbol(const Symbol& sym, const ast::Item* declaration, diagnostic::DiagnosticCollector* diagnostics) {
    symbols.push_back(sym);
    switch (sym.kind) {
        case SymbolKind::Struct:
        case SymbolKind::BuiltinType:
            if (type_names.count(sym.name)) {
                diagnostics->add_entry(
                    diagnostic::Severity::Error,
                    declaration->span.begin,
                    "redefinition of type " + sym.name
                );
                return;
            }
            type_names[sym.name] = sym.id;
            break;
        case SymbolKind::Function:
        case SymbolKind::Constant:
        case SymbolKind::BuiltinValue:
            if (value_names.count(sym.name)) {
                diagnostics->add_entry(
                    diagnostic::Severity::Error,
                    declaration->span.begin,
                    "redefinition of value " + sym.name
                );
                return;
            }
            value_names[sym.name] = sym.id;
            break;
    }
    if (sym.declaration) {
        item_symbols[sym.declaration] = sym.id;
    }
}

void CrateIndex::addImpl(const ast::ImplItem* impl) {
    pending_impls.push_back(impl);
}

void CrateIndex::addBuiltinSymbols() {
    for (const auto& sym: builtins) {
        addSymbol(sym, nullptr, nullptr);
    }
}

void CrateIndex::dump(std::ostream& os) const {
    for (const auto& sym: symbols) {
        os << "Symbol name=" << sym.name << '\n';
        os << "       id=" << sym.id << '\n';
        os << "       kind=";
        switch (sym.kind) {
            case SymbolKind::Struct:
                os << "Struct\n"; break;
            case SymbolKind::Function:
                os << "Function\n"; break;
            case SymbolKind::Constant:
                os << "Constant\n"; break;
            case SymbolKind::BuiltinType:
                os << "BuiltinType\n"; break;
            case SymbolKind::BuiltinValue:
                os << "BuiltinValue\n"; break;
        }
        os << '\n';
    }
}

CrateIndex collectDeclarations(const ast::Crate *crate, diagnostic::DiagnosticCollector* diagnostics) {
    CrateIndex index;
    for (const auto& item: crate->items) {
        if (const auto* func = dynamic_cast<ast::FunctionItem*>(item.get())) {
            index.addSymbol(
                Symbol {
                    index.nextId(),
                    SymbolKind::Function,
                    func->name,
                    item.get()
                },
                item.get(),
                diagnostics
            );
        } else if (const auto* stru = dynamic_cast<ast::StructItem*>(item.get())) {
            index.addSymbol(
                Symbol {
                    index.nextId(),
                    SymbolKind::Struct,
                    stru->name,
                    item.get()
                },
                item.get(),
                diagnostics
            );
        } else if (const auto* cons = dynamic_cast<ast::ConstantItem*>(item.get())) {
            index.addSymbol(
                Symbol {
                    index.nextId(),
                    SymbolKind::Constant,
                    cons->name,
                    item.get()
                },
                item.get(),
            diagnostics
            );
        } else if (const auto* impl = dynamic_cast<ast::ImplItem*>(item.get())) {
            index.addImpl(impl);
        }
    }
    return index;
}

} // namespace semantic