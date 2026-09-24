#include "dump.hpp"
#include "ast.hpp"
#define PRINT(os, depth) printIndent(depth, os); os
#define INDENT(os, depth) printIndent(depth, os);

namespace ast {

void printIndent(int depth, std::ostream& os) {
    for (int i = 0; i < depth; i++) {
        os << '\t';
    }
}

void dumpFunction(const FunctionItem& ast, std::ostream& os, int depth) {
    PRINT(os, depth) << "FunctionItem name=" << ast.name << '\n';
    if (ast.generic_params.size()) dumpGenericParams(ast.generic_params, os, depth + 1);
    if (ast.self_param.has_value()) dumpSelfParam(*ast.self_param, os, depth + 1);
    if (ast.function_params.size()) dumpFunctionParams(ast.function_params, os, depth + 1);
    if (ast.return_type) dumpTypeRef(ast.return_type, os, depth + 1);
    if (ast.where_clause.has_value()) dumpWhereClause(*ast.where_clause, os, depth + 1);
    dumpBlockExpression(*ast.body, os, depth + 1);
}

void dumpGenericParams(const std::vector<GenericParam> &ast, std::ostream &os, int depth) {
    for (const auto& param: ast) {
        dumpGenericParam(param, os, depth);
    }
}

void dumpGenericParam(const GenericParam &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "GenericParam\n";
    dumpLifetime(ast.lifetime, os, depth + 1);
    if (ast.lifetime_bounds.has_value()) dumpLifetimeBounds(*ast.lifetime_bounds, os, depth + 1); 
}

void dumpLifetime(const Lifetime& ast, std::ostream& os, int depth) {
    PRINT(os, depth) << "Lifetime name=" << ast.name << '\n';
}

void dumpLifetimeBounds(const LifetimeBounds &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "LifetimeBounds\n";
    for (const auto& lifetime: ast.lifetimes) {
        dumpLifetime(lifetime, os, depth + 1);
    }
}

void dumpSelfParam(const SelfParam &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "SelfParam";
    if (ast.is_ref) os << " & ";
    if (ast.is_mut) os << " mut ";
    os << '\n';
    if (ast.lifetime.has_value()) dumpLifetime(*ast.lifetime, os, depth + 1);
}

void dumpFunctionParams(const std::vector<FunctionParam> &ast, std::ostream &os, int depth) {
    for (const auto& param: ast) {
        dumpFunctionParam(param, os, depth);
    }
}

void dumpFunctionParam(const FunctionParam &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "FunctionParam name=" << ast.name << (ast.is_mut ? " mut" : "") << '\n';
    if (ast.type) dumpTypeRef(ast.type, os, depth + 1);
}

void dumpTypeRef(const AstPtr<TypeRef> &ast, std::ostream &os, int depth) {
    if (const auto* unit = dynamic_cast<UnitType*>(ast.get())) {
        return dumpUnitType(unit, os, depth);
    } else if (const auto* path = dynamic_cast<TypePath*>(ast.get())) {
        return dumpTypePath(path, os, depth);
    } else if (const auto* ref = dynamic_cast<ReferenceType*>(ast.get())) {
        return dumpReferenceType(ref, os, depth);
    } else if (const auto* arr = dynamic_cast<ArrayType*>(ast.get())) {
        return dumpArrayType(arr, os, depth + 1);
    } else if (const auto* paren = dynamic_cast<ParenthesizedType*>(ast.get())) {
        return dumpParenthesizedType(paren, os, depth + 1);
    }
    throw std::logic_error("unexpected type ref");
}

#undef PRINT
#undef INDENT

} // namespace ast