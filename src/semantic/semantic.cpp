#include "semantic.hpp"
#include "ast/ast.hpp"
#include "diagnostic/diagnostic.hpp"
#include "semantic/semantic_model.hpp"
#include "semantic/symbol.hpp"
#include "semantic/type_resolver.hpp"

namespace semantic {

SemanticResult analyze(const ast::Crate& crate) {
    diagnostic::DiagnosticCollector collector;
    CrateIndex index = collectDeclarations(&crate, &collector);
    SemanticModel model;
    StructResolver struct_resolver;
    struct_resolver.declareAll(index, model);
    // TODO unimplemented

    return SemanticResult {
        !collector.has_error(),
        std::make_unique<SemanticModel>(model),
        collector.diagnostics()
    };
}

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