#include "generic_arguments.hpp"
#include "diagnostic/diagnostic.hpp"

namespace semantic {

bool checkNoGenericArguments(const ast::GenericArgs* args, std::string_view subject, diagnostic::DiagnosticCollector& diag) {
    if (!args || args->args.empty()) {
        return true;
    }
    diag.add_entry(diagnostic::Severity::Error, args->span.begin, std::string(subject) + " takes no generic arguments");
    return false;
}

bool checkLifetimeOnlyArguments(const ast::GenericArgs* args, std::string_view subject, diagnostic::DiagnosticCollector& diag) {
    if (!args) {
        return true;
    }
    for (const auto& arg: args->args) {
        if (arg.type.has_value() || !arg.lifetime.has_value()) {
            diag.add_entry(diagnostic::Severity::Error, arg.span.begin, std::string(subject) + "takes only lifetime arguments");
            return false;
        }
    }
    return true;
}

const ast::TypeRef* requireSingleTypeArgument(const ast::GenericArgs* args, ast::SourceSpan span, std::string_view subject, diagnostic::DiagnosticCollector& diag) {
    if (!args || args->args.size() != 1) {
        diag.add_entry(diagnostic::Severity::Error, span.begin, std::string(subject) + " requires exactly one type argument");
        return nullptr;
    }
    const auto& arg = args->args.front();
    if (arg.lifetime.has_value() || !arg.type.has_value() || !*arg.type) {
        diag.add_entry(diagnostic::Severity::Error, span.begin, std::string(subject) + " requires a type argument");
        return nullptr;
    }
    return arg.type->get();
}

} // namespace semantic