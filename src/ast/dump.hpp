#pragma once
#include "ast.hpp"
#include <iostream>
#include <ostream>

namespace ast {

void dumpAst(const Crate& root, std::ostream& os);

void printIndent(int depth, std::ostream& os);

void dumpFunction(const FunctionItem& ast, std::ostream& os, int depth);
void dumpStruct(const StructItem& ast, std::ostream& os, int depth);
void dumpConstant(const ConstantItem& ast, std::ostream& os, int depth);
void dumpImpl(const ImplItem& ast, std::ostream& os, int depth);

void dumpGenericParams(const std::vector<GenericParam>& ast, std::ostream& os, int depth);
void dumpGenericParam(const GenericParam& ast, std::ostream& os, int depth);

void dumpLifetime(const Lifetime& ast, std::ostream& os, int depth);
void dumpLifetimeBounds(const LifetimeBounds& ast, std::ostream& os, int depth);

void dumpSelfParam(const SelfParam& ast, std::ostream& os, int depth);
void dumpFunctionParams(const std::vector<FunctionParam>& ast, std::ostream& os, int depth);
void dumpFunctionParam(const FunctionParam& ast, std::ostream& os, int depth);

void dumpTypeRef(const AstPtr<TypeRef>& ast, std::ostream& os, int depth);
void dumpUnitType(const UnitType* ast, std::ostream& os, int depth);
void dumpTypePath(const TypePath* ast, std::ostream& os, int depth);
void dumpReferenceType(const ReferenceType* ast, std::ostream& os, int depth);
void dumpArrayType(const ArrayType* ast, std::ostream& os, int depth);
void dumpParenthesizedType(const ParenthesizedType* ast, std::ostream& os, int depth);

void dumpWhereClause(const WhereClause& ast, std::ostream& os, int depth);

void dumpBlockExpression(const BlockExpression& ast, std::ostream& os, int depth);

} // namespace ast