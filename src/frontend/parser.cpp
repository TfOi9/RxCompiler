#include "parser.hpp"
#include "../ast/builder.hpp"
#include "../ast/dump.hpp"
#include "../generated/RxLexer.h"
#include "../generated/RxParser.h"

namespace frontend {

ParseResult parseToAst(std::string_view source) {
    antlr4::ANTLRInputStream input(source);

    RxLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);

    std::unique_ptr<diagnostic::DiagnosticCollector> collector(new diagnostic::DiagnosticCollector());
    auto collector_view = collector.get();
    diagnostic::AntlrErrorListener listener(std::move(collector));

    lexer.removeErrorListeners();
    lexer.addErrorListener(&listener);

    tokens.fill();
    
    RxParser parser(&tokens);
    parser.removeErrorListeners();
    parser.addErrorListener(&listener);

    auto* tree = parser.crate();

    if (collector_view->has_error() || parser.getNumberOfSyntaxErrors() > 0) {
        return ParseResult {
            nullptr,
            collector_view->diagnostics(),
            false
        };
    }

    ast::AstBuilder builder(source);
    return ParseResult {
        builder.build(tree),
        std::vector<diagnostic::Diagnostic>(),
        true
    };
}

void dump(const ParseResult &result, std::ostream& os) {
    if (result.success) {
        os << "Parse Successed.\n";
        if (result.crate) ast::dumpAst(*result.crate, os);
    } else {
        os << "Parse Failed.\n";
        for (const auto& diag: result.diagnostics) {
            diagnostic::dumpDiagnostic(diag, os);
        }
    }
}

} // namespace frontend