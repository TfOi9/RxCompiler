#include "integer_literal.hpp"

namespace semantic {

bool isIntegerKind(PrimaryTyKind kind) {
    return kind == PrimaryTyKind::I32 || kind == PrimaryTyKind::U32 ||
        kind == PrimaryTyKind::ISize || kind == PrimaryTyKind::USize;
}

bool isSignedKind(PrimaryTyKind kind) {
    return kind == PrimaryTyKind::I32 || kind == PrimaryTyKind::ISize;
}

std::optional<PrimaryTyKind> suffixKind(ast::IntegerSuffix suffix) {
    switch (suffix) {
        case ast::IntegerSuffix::I32: return PrimaryTyKind::I32;
        case ast::IntegerSuffix::U32: return PrimaryTyKind::U32;
        case ast::IntegerSuffix::Isize: return PrimaryTyKind::ISize;
        case ast::IntegerSuffix::Usize: return PrimaryTyKind::USize;
        case ast::IntegerSuffix::None: return std::nullopt;
    }
    return std::nullopt;
}

std::string_view suffixSpelling(ast::IntegerSuffix suffix) {
    switch (suffix) {
        case ast::IntegerSuffix::I32: return "i32";
        case ast::IntegerSuffix::U32: return "u32";
        case ast::IntegerSuffix::Isize: return "isize";
        case ast::IntegerSuffix::Usize: return "usize";
        case ast::IntegerSuffix::None: return {};
    }
    return {};
}

bool parseIntegerMagnitude(const ast::IntegerLiteralValue& literal, uint64_t& result, std::string& error) {
    std::string_view digits(literal.spelling);
    const auto suffix = suffixSpelling(literal.suffix);
    if (!suffix.empty()) {
        if (digits.size() <= suffix.size() || digits.substr(digits.size() - suffix.size()) != suffix) {
            error = "integer suffix does not match literal spelling";
            return false;
        }
        digits.remove_suffix(suffix.size());
    }

    unsigned int base = 10;
    if (digits.size() >= 2 && digits[0] == '0') {
        if (digits[1] == 'b') {
            base = 2;
            digits.remove_prefix(2);
        } else if (digits[1] == 'o') {
            base = 8;
            digits.remove_prefix(2);
        } else if (digits[1] == 'x') {
            base = 16;
            digits.remove_prefix(2);
        }
    }

    bool has_digit = false;
    uint64_t value = 0;
    for (char ch: digits) {
        if (ch == '_') {
            continue;
        }
        unsigned int digit = 0;
        if (ch >= '0' && ch <= '9') {
            digit = static_cast<unsigned int>(ch - '0');
        } else if (ch >= 'a' && ch <= 'f') {
            digit = static_cast<unsigned int>(ch - 'a') + 10;
        } else if (ch >= 'A' && ch <= 'F') {
            digit = static_cast<unsigned int>(ch - 'A') + 10;
        } else {
            error = "invalid digit in integer literal";
            return false;
        }
        if (digit >= base) {
            error = "digit is not valid for the integer literal radix";
            return false;
        }
        if (value > (std::numeric_limits<uint64_t>::max() - digit) / base) {
            error = "integer literal magnitude is too large to represent";
            return false;
        }
        value = value * base + digit;
        has_digit = true;
    }
    if (!has_digit) {
        error = "integer literal has no digits";
        return false;
    }
    result = value;
    return true;
}

} // namespace semantic