#pragma once
#include "../ast/ast.hpp"
#include <string>
#include <vector>

namespace diagnostic {

enum class Severity {
    Error,
    Warning
};

struct Diagnostic {
    Severity severity;
    ast::SourceSpan span;
    std::string message;
};

class DiagnosticBase {
public:
    virtual ~DiagnosticBase() = default;
    virtual void add_entry(Severity severity, ast::SourceSpan span, const std::string& message) = 0;
    virtual bool has_error() = 0;
};

class DiagnosticCollector final: public DiagnosticBase {
public:
    ~DiagnosticCollector() override = default;
    void add_entry(Severity severity, ast::SourceSpan span, const std::string& message) override;
    bool has_error() override;
    std::vector<Diagnostic> diagnostics();

private:
    std::vector<Diagnostic> diagnostics_;
    bool has_error_ = false;
};

} // namespace diagnostic