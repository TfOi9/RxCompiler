#pragma once
#include "../ast/ast.hpp"
#include "antlr4-runtime.h"
#include <string>
#include <vector>
#include <iostream>

namespace diagnostic {

enum class Severity {
    Error,
    Warning
};

struct Diagnostic {
    Severity severity;
    ast::SourceLocation location;
    std::string message;
};

void dumpDiagnostic(const Diagnostic& diag, std::ostream& os);

class DiagnosticBase {
public:
    virtual ~DiagnosticBase() = default;
    virtual void add_entry(Severity severity, ast::SourceLocation location, const std::string& message) = 0;
    virtual bool has_error() = 0;
};

class DiagnosticCollector final: public DiagnosticBase {
public:
    ~DiagnosticCollector() override = default;
    void add_entry(Severity severity, ast::SourceLocation location, const std::string& message) override;
    bool has_error() override;
    std::vector<Diagnostic> diagnostics();

private:
    std::vector<Diagnostic> diagnostics_;
    bool has_error_ = false;
};

class AntlrErrorListener final: public antlr4::BaseErrorListener {
public:
    explicit AntlrErrorListener(std::unique_ptr<DiagnosticBase> diag): diag_(std::move(diag)) {}
    void syntaxError(
        antlr4::Recognizer*,
        antlr4::Token* offendingSymbol,
        size_t line,
        size_t charPositionInLine,
        const std::string& message,
        std::exception_ptr
    ) override;

private:
    std::unique_ptr<DiagnosticBase> diag_;
};

} // namespace diagnostic