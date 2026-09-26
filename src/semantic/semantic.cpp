#include "semantic.hpp"
#include "ast/ast.hpp"
#include "semantic/symbol.hpp"

namespace semantic {

IndexResult index(const ast::Crate& crate) {
    diagnostic::DiagnosticCollector collector;
    CrateIndex index = collectDeclarations(&crate, &collector);
    return IndexResult {
        !collector.has_error(),
        std::move(index),
        collector.diagnostics()
    };
}

void dump(const IndexResult &result, std::ostream& os) {
    if (result.success) {
        os << "Index Successed.\n";
        result.index.dump(os);
    } else {
        os << "Index Failed.\n";
        for (const auto& diag: result.diagnostics) {
            diagnostic::dumpDiagnostic(diag, os);
        }
    }
}

} // namespace semantic