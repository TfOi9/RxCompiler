#pragma once
#include "../ast/ast.hpp"
#include "../diagnostic/diagnostic.hpp"
#include <string_view>
#include <memory>
#include <iostream>

namespace frontend {

struct ParseResult {
    std::unique_ptr<ast::Crate> crate;
    std::vector<diagnostic::Diagnostic> diagnostics;
    bool success;
};

ParseResult parseToAst(std::string_view source);
void dump(const ParseResult& result, std::ostream& os);

} // namespace frontend