#pragma once
#include "ast/ast.hpp"
#include "types.hpp"
#include <optional>

namespace semantic {

bool isIntegerKind(PrimaryTyKind kind);
bool isSignedKind(PrimaryTyKind kind);

std::optional<PrimaryTyKind> suffixKind(ast::IntegerSuffix suffix);
std::string_view suffixSpelling(ast::IntegerSuffix suffix);

bool parseIntegerMagnitude(const ast::IntegerLiteralValue& literal, uint64_t& result, std::string& error);

PrimaryTyKind selectIntegerKind(ast::IntegerSuffix suffix, std::optional<PrimaryTyKind> expected);
std::optional<PrimaryTyKind> expectedIntegerKind(const TypeContext& types, std::optional<TyId> expected);

bool checkPositiveIntegerMagnitude(PrimaryTyKind kind, uint64_t magnitude) noexcept;
bool checkNegatedIntegerMagnitude(PrimaryTyKind kind, uint64_t magnitude) noexcept;

} // namespace semantic