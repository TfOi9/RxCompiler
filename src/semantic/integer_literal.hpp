#pragma once
#include "ast/ast.hpp"
#include "types.hpp"

namespace semantic {

bool isIntegerKind(PrimaryTyKind kind);
bool isSignedKind(PrimaryTyKind kind);

std::optional<PrimaryTyKind> suffixKind(ast::IntegerSuffix suffix);
std::string_view suffixSpelling(ast::IntegerSuffix suffix);

bool parseIntegerMagnitude(const ast::IntegerLiteralValue& literal, uint64_t& result, std::string& error);

} // namespace semantic