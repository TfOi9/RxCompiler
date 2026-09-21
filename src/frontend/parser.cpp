#include "parser.hpp"
#include "antlr4-runtime.h"
#include "../generated/RxLexer.h"
#include "../generated/RxParser.h"
#include <vector>

namespace frontend {

ParseResult parseToAst(std::string_view source) {
    antlr4::ANTLRInputStream input(source);

    RxLexer lexer(&input);
    antlr4::CommonTokenStream tokens(&lexer);

    std::unique_ptr<diagnostic::DiagnosticCollector> collector(new diagnostic::DiagnosticCollector());
    diagnostic::AntlrErrorListener listener(std::move(collector));

    lexer.removeErrorListeners();
    lexer.addErrorListener(&listener);

    tokens.fill();
    
    RxParser parser(&tokens);
    parser.removeErrorListeners();
    parser.addErrorListener(&listener);

    auto* tree = parser.crate();

    if (collector->has_error() || parser.getNumberOfSyntaxErrors() > 0) {
        return ParseResult {
            nullptr,
            collector->diagnostics()
        };
    }

    // call AST builder
    // ast::AstBuilder builder(source);
    return ParseResult {
        /* builder.build(tree), */ nullptr,
        std::vector<diagnostic::Diagnostic>()
    };
}

} // namespace frontend