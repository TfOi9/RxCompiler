#pragma once
#include "ast/ast.hpp"
#include "diagnostic/diagnostic.hpp"
#include <string_view>

namespace semantic {

bool checkNoGenericArguments(const ast::GenericArgs* args, std::string_view subject, diagnostic::DiagnosticCollector& diag);
bool checkLifetimeOnlyArguments(const ast::GenericArgs* args, std::string_view subject, diagnostic::DiagnosticCollector& diag);
const ast::TypeRef* requireSingleTypeArgument(const ast::GenericArgs* args, ast::SourceSpan span, std::string_view subject, diagnostic::DiagnosticCollector& diag);

} // namespace semantic