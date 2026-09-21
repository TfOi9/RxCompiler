#include "diagnostic.hpp"

namespace diagnostic {

void DiagnosticCollector::add_entry(Severity severity, ast::SourceLocation location, const std::string& message) {
    diagnostics_.push_back(
        Diagnostic {
            severity,
            location,
            message
        }
    );
    if (severity == Severity::Error) {
        has_error_ = true;
    }
}

bool DiagnosticCollector::has_error() {
    return has_error_;
}

std::vector<Diagnostic> DiagnosticCollector::diagnostics() {
    return diagnostics_;
}

void AntlrErrorListener::syntaxError(
    antlr4::Recognizer*,
    antlr4::Token* offendingSymbol,
    size_t line,
    size_t charPositionInLine,
    const std::string& message,
    std::exception_ptr
) {
    size_t offset = 0;
    if (offendingSymbol != nullptr) {
        offset = offendingSymbol->getStartIndex();
    }
    diag_->add_entry(
        Severity::Error,
        ast::SourceLocation{
            offset,
            line,
            charPositionInLine + 1},
        message
    );
}

} // namespace diagnostic