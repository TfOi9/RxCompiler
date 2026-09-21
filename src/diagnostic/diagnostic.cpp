#include "diagnostic.hpp"

namespace diagnostic {

void DiagnosticCollector::add_entry(Severity severity, ast::SourceSpan span, const std::string& message) {
    diagnostics_.push_back(
        Diagnostic {
            severity,
            span,
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

} // namespace diagnostic