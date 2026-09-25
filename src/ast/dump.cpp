#include "dump.hpp"
#include "ast.hpp"
#include <stdexcept>
#define PRINT(os, depth) printIndent(depth, os); os
#define INDENT(os, depth) printIndent(depth, os);

namespace ast {

void dumpAst(const Crate& root, std::ostream& os) {
    PRINT(os, 0) << "Crate\n";
    for (const auto& item: root.items) {
        if (item) dumpItem(item, os, 1);
    }
}

void printIndent(int depth, std::ostream& os) {
    for (int i = 0; i < depth; i++) {
        os << '\t';
    }
}

void dumpItem(const AstPtr<Item> &ast, std::ostream &os, int depth) {
    if (const auto* func = dynamic_cast<FunctionItem*>(ast.get())) {
        dumpFunction(*func, os, depth);
    } else if (const auto* stru = dynamic_cast<StructItem*>(ast.get())) {
        dumpStruct(*stru, os, depth);
    } else if (const auto* cons = dynamic_cast<ConstantItem*>(ast.get())) {
        dumpConstant(*cons, os, depth);
    } else if (const auto* impl = dynamic_cast<ImplItem*>(ast.get())) {
        dumpImpl(*impl, os, depth);
    } else throw std::logic_error("unexpected item type");
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

void dumpStruct(const StructItem &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "StructItem name=" << ast.name << '\n';
    for (const auto& attr: ast.attributes) {
        dumpOuterAttributes(attr, os, depth + 1);
    }
    for (const auto& param: ast.generic_params) {
        dumpGenericParam(param, os, depth + 1);
    }
    if (ast.where_clause.has_value()) dumpWhereClause(*ast.where_clause, os, depth + 1);
    for (const auto& field: ast.struct_fields) {
        dumpStructField(field, os, depth + 1);
    }
}

void dumpConstant(const ConstantItem &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "ConstantItem name=" << ast.name << '\n';
    if (ast.type) dumpTypeRef(ast.type, os, depth + 1);
    if (ast.value) dumpConstValue(*ast.value, os, depth + 1);
}

void dumpImpl(const ImplItem &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "ImplItem\n";
    if (ast.generic_params.size()) dumpGenericParams(ast.generic_params, os, depth + 1);
    if (ast.type) dumpTypeRef(ast.type, os, depth + 1);
    if (ast.where_clause) dumpWhereClause(*ast.where_clause, os, depth + 1);
    if (ast.associated_items.size()) dumpAssociatedItems(ast.associated_items, os, depth + 1);
}

void dumpOuterAttributes(const OuterAttribute &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "OuterAttribute\n";
    for (const auto& name: ast.derive_names) {
        switch (name) {
            case DeriveName::Copy:
                PRINT(os, depth + 1) << "Copy\n"; break;
            case DeriveName::Clone:
                PRINT(os, depth + 1) << "Clone\n"; break;
            case DeriveName::PartialEq:
                PRINT(os, depth + 1) << "PartialEq\n"; break;
            case DeriveName::Eq:
                PRINT(os, depth + 1) << "Eq\n"; break;
        }
    }
}

void dumpStructField(const StructField &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "StructField name=" << ast.name << '\n';
    if (ast.type) dumpTypeRef(ast.type, os, depth + 1);
}

void dumpAssociatedItems(const std::vector<AssociatedItem> &ast, std::ostream &os, int depth) {
    for (const auto& item: ast) {
        dumpAssociatedItem(item, os, depth);
    }
}

void dumpAssociatedItem(const AssociatedItem &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "AssociatedItem\n";
    if (ast.constant.has_value() && *ast.constant) dumpConstant(**ast.constant, os, depth + 1);
    if (ast.function.has_value() && *ast.function) dumpFunction(**ast.function, os, depth + 1);
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
        return dumpArrayType(arr, os, depth);
    } else if (const auto* paren = dynamic_cast<ParenthesizedType*>(ast.get())) {
        return dumpParenthesizedType(paren, os, depth);
    }
    throw std::logic_error("unexpected type ref");
}

void dumpUnitType(const UnitType *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "UnitType\n";
}

void dumpTypePath(const TypePath *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "TypePath\n";
    for (const auto& seg: ast->path_segments) {
        dumpTypePathSegment(seg, os, depth + 1);
    }
}

void dumpReferenceType(const ReferenceType *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "ReferenceType" << (ast->is_mut ? " mut" : "") << '\n';
    if (ast->lifetime) dumpLifetime(ast->lifetime.value(), os, depth + 1);
    if (ast->type) dumpTypeRef(ast->type, os, depth + 1);
}

void dumpArrayType(const ArrayType *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "ArrayType\n";
    if (ast->type) dumpTypeRef(ast->type, os, depth + 1);
    if (ast->length) dumpConstValue(*ast->length, os, depth + 1);
}

void dumpParenthesizedType(const ParenthesizedType *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "ParenthesizedType\n";
    if (ast->type) dumpTypeRef(ast->type, os, depth + 1);
}

void dumpTypePathSegment(const TypePathSegment &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "TypePathSegment\n";
    dumpPathIdentSegment(ast.ident_segment, os, depth + 1);
    if (ast.generic_args.has_value()) dumpGenericArgs(*ast.generic_args, os, depth + 1);
}

void dumpPathIdentSegment(const PathIdentSegment &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "PathIdentSegment";
    if (ast.name.has_value()) os << " name=" << *ast.name;
    if (ast.is_self) os << " self";
    if (ast.is_Self) os << " Self";
    os << '\n';
}

void dumpGenericArgs(const GenericArgs &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "GenericArgs\n";
    for (const auto& arg: ast.args) {
        dumpGenericArg(arg, os, depth + 1);
    }
}

void dumpGenericArg(const GenericArg& ast, std::ostream& os, int depth) {
    PRINT(os, depth) << "GenericArg\n";
    if (ast.lifetime.has_value()) dumpLifetime(*ast.lifetime, os, depth + 1);
    if (ast.type.has_value() && ast.type.value()) dumpTypeRef(*ast.type, os, depth + 1);
}

void dumpWhereClause(const WhereClause &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "WhereClause\n";
    for (const auto& item: ast.items) {
        if (item.lifetime.has_value()) dumpLifetimeWhereClauseItem(*item.lifetime, os, depth + 1);
        if (item.type_bound.has_value()) dumpTypeBoundWhereClauseItem(*item.type_bound, os, depth + 1);
    }
}

void dumpLifetimeWhereClauseItem(const LifetimeWhereClauseItem &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "LifetimeWhereClauseItem\n";
    dumpLifetime(ast.lifetime, os, depth + 1);
    dumpLifetimeBounds(ast.lifetime_bounds, os, depth + 1);
}

void dumpTypeBoundWhereClauseItem(const TypeBoundWhereClauseItem &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "TypeBoundWhereClauseItem\n";
    if (ast.type) dumpTypeRef(ast.type, os, depth + 1);
    if (ast.type_param_bounds.has_value()) dumpTypeParamBounds(*ast.type_param_bounds, os, depth + 1);
}

void dumpTypeParamBounds(const TypeParamBounds &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "TypeParamBounds\n";
    for (const auto& lifetime: ast.lifetimes) {
        dumpLifetime(lifetime, os, depth + 1);
    }
}

void dumpBlockExpression(const BlockExpression &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "BlockExpression\n";
    for (const auto& stmt: ast.statements) {
        dumpStatement(stmt, os, depth + 1);
    }
    if (ast.tail_expression) dumpExpression(ast.tail_expression, os, depth + 1);
}

void dumpStatement(const AstPtr<Statement>& ast, std::ostream& os, int depth) {
    if (const auto* let = dynamic_cast<LetStatement*>(ast.get())) {
        dumpLetStatement(let, os, depth);
    } else if (const auto* expr = dynamic_cast<ExpressionStatement*>(ast.get())) {
        dumpExpressionStatement(expr, os, depth);
    } else if (const auto* emp = dynamic_cast<EmptyStatement*>(ast.get())) {
        PRINT(os, depth) << "EmptyStatement\n";
    } else throw std::logic_error("unexpected statement type");
}

void dumpLetStatement(const LetStatement *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "LetStatement\n";
    dumpIdentifierBinding(ast->identifier_binding, os, depth + 1);
    if (ast->type) dumpTypeRef(ast->type, os, depth + 1);
    if (ast->expression) dumpExpression(ast->expression, os, depth + 1);
}

void dumpIdentifierBinding(const IdentifierBinding &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "IdentifierBinding name=" << ast.name << (ast.is_mut ? " mut" : "") << '\n';
}

void dumpExpressionStatement(const ExpressionStatement *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "ExpressionStatement\n";
    if (ast->expr) dumpExpression(ast->expr, os, depth + 1);
}

void dumpConstValue(const ConstValue &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "ConstValue\n";
    if (ast.integer.has_value()) dumpIntegerLiteralValue(*ast.integer, os, depth + 1);
    if (ast.boolean.has_value()) PRINT(os, depth + 1) << "Bool value=" << (*ast.boolean ? "true" : "false") << '\n';
    if (ast.path.has_value()) dumpPathInExpression(*ast.path, os, depth + 1);
    if (ast.magnitude) dumpMagnitude(*ast.magnitude, os, depth + 1);
    if (ast.inner) dumpConstValue(*ast.inner, os, depth + 1);
}

void dumpIntegerLiteralValue(const IntegerLiteralValue &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "IntegerLiteralValue spelling=" << ast.spelling << " suffix=";
    switch (ast.suffix) {
        case IntegerSuffix::I32:
            os << "i32"; break;
        case IntegerSuffix::U32:
            os << "u32"; break;
        case IntegerSuffix::Isize:
            os << "isize"; break;
        case IntegerSuffix::Usize:
            os << "usize"; break;
        case IntegerSuffix::None:
            os << "none"; break;
    }
    os << '\n';
}

void dumpPathInExpression(const PathInExpression &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "PathInExpression\n";
    for (const auto& seg: ast.segments) {
        dumpPathExprSegment(seg, os, depth + 1);
    }
}

void dumpPathExprSegment(const PathExprSegment &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "PathExprSegment\n";
    dumpPathIdentSegment(ast.ident_segment, os, depth + 1);
    if (ast.generic_args.has_value()) dumpGenericArgs(*ast.generic_args, os, depth + 1);
}

void dumpMagnitude(const Magnitude &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "Magnitude\n";
    if (ast.value.has_value()) dumpIntegerLiteralValue(*ast.value, os, depth + 1);
    if (ast.path.has_value()) dumpPathInExpression(*ast.path, os, depth + 1);
    if (ast.inner) dumpMagnitude(*ast.inner, os, depth + 1);
}

void dumpExpression(const AstPtr<Expression>& ast, std::ostream& os, int depth) {
    if (const auto* block = dynamic_cast<BlockExpression*>(ast.get())) {
        dumpBlockExpression(*block, os, depth);
    } else if (const auto* integer = dynamic_cast<IntegerExpression*>(ast.get())) {
        dumpIntegerExpression(integer, os, depth);
    } else if (const auto* boolean = dynamic_cast<BoolExpression*>(ast.get())) {
        dumpBoolExpression(boolean, os, depth);
    } else if (const auto* unit = dynamic_cast<UnitExpression*>(ast.get())) {
        dumpUnitExpression(unit, os, depth);
    } else if (const auto* unary = dynamic_cast<UnaryExpression*>(ast.get())) {
        dumpUnaryExpression(unary, os, depth);
    } else if (const auto* binary = dynamic_cast<BinaryExpression*>(ast.get())) {
        dumpBinaryExpression(binary, os, depth);
    } else if (const auto* assign = dynamic_cast<AssignmentExpression*>(ast.get())) {
        dumpAssignmentExpression(assign, os, depth);
    } else if (const auto* path = dynamic_cast<PathExpression*>(ast.get())) {
        dumpPathExpression(path, os, depth);
    } else if (const auto* call = dynamic_cast<CallExpression*>(ast.get())) {
        dumpCallExpression(call, os, depth);
    } else if (const auto* arr = dynamic_cast<ArrayExpression*>(ast.get())) {
        dumpArrayExpression(arr, os, depth);
    } else if (const auto* index = dynamic_cast<IndexExpression*>(ast.get())) {
        dumpIndexExpression(index, os, depth);
    } else if (const auto* field = dynamic_cast<FieldExpression*>(ast.get())) {
        dumpFieldExpression(field, os, depth);
    } else if (const auto* method = dynamic_cast<MethodCallExpression*>(ast.get())) {
        dumpMethodCallExpression(method, os, depth);
    } else if (const auto* cast = dynamic_cast<CastExpression*>(ast.get())) {
        dumpCastExpression(cast, os, depth);
    } else if (const auto* stru = dynamic_cast<StructExpression*>(ast.get())) {
        dumpStructExpression(stru, os, depth);
    } else if (const auto* ife = dynamic_cast<IfExpression*>(ast.get())) {
        dumpIfExpression(ife, os, depth);
    } else if (const auto* loop = dynamic_cast<LoopExpression*>(ast.get())) {
        dumpLoopExpression(loop, os, depth);
    } else if (const auto* whilee = dynamic_cast<WhileExpression*>(ast.get())) {
        dumpWhileExpression(whilee, os, depth);
    } else if (const auto* breake = dynamic_cast<BreakExpression*>(ast.get())) {
        dumpBreakExpression(breake, os, depth);
    } else if (const auto* cont = dynamic_cast<ContinueExpression*>(ast.get())) {
        dumpContinueExpression(cont, os, depth);
    } else if (const auto* retu = dynamic_cast<ReturnExpression*>(ast.get())) {
        dumpReturnExpression(retu, os, depth);
    } else if (const auto* grouped = dynamic_cast<GroupedExpression*>(ast.get())) {
        dumpGroupedExpression(grouped, os, depth);
    } else throw std::logic_error("unexpected expression type");
}

void dumpIntegerExpression(const IntegerExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "IntegerExpression\n";
    dumpIntegerLiteralValue(ast->value, os, depth + 1);
}

void dumpBoolExpression(const BoolExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "BoolExpression value=" << (ast->value ? "true" : "false") << '\n';
}

void dumpUnitExpression(const UnitExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "UnitExpression\n";
}

void dumpUnaryExpression(const UnaryExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "UnaryExpression op=";
    dumpUnaryOperator(ast->op, os);
    os << '\n';
    if (ast->operand) dumpExpression(ast->operand, os, depth + 1);
}

void dumpBinaryExpression(const BinaryExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "BinaryExpression op=";
    dumpBinaryOperator(ast->op, os);
    os << '\n';
    if (ast->lhs_operand) dumpExpression(ast->lhs_operand, os, depth + 1);
    if (ast->rhs_operand) dumpExpression(ast->rhs_operand, os, depth + 1);
}

void dumpAssignmentExpression(const AssignmentExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "AssignmentExpression op=";
    dumpAssignmentOperator(ast->op, os);
    os << '\n';
    if (ast->lhs_operand) dumpExpression(ast->lhs_operand, os, depth + 1);
    if (ast->rhs_operand) dumpExpression(ast->rhs_operand, os, depth + 1);
}

void dumpPathExpression(const PathExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "PathExpression\n";
    dumpPathInExpression(ast->path, os, depth + 1);
}

void dumpCallExpression(const CallExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "CallExpression\n";
    if (ast->callee) dumpExpression(ast->callee, os, depth + 1);
    for (const auto& arg: ast->args) {
        if (arg) dumpExpression(arg, os, depth + 1);
    }
}

void dumpArrayExpression(const ArrayExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "ArrayExpression\n";
    for (const auto& elem: ast->elements) {
        dumpExpression(elem, os, depth + 1);
    }
    if (ast->repeated_length) dumpConstValue(*ast->repeated_length, os, depth + 1);
}

void dumpIndexExpression(const IndexExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "IndexExpression\n";
    if (ast->base) dumpExpression(ast->base, os, depth + 1);
    if (ast->index) dumpExpression(ast->index, os, depth + 1);
}

void dumpFieldExpression(const FieldExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "FieldExpression field_name=" << ast->field_name << '\n';
    if (ast->base) dumpExpression(ast->base, os, depth + 1);
}

void dumpMethodCallExpression(const MethodCallExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "MethodCallExpression\n";
    if (ast->receiver) dumpExpression(ast->receiver, os, depth + 1);
    dumpPathExprSegment(ast->method, os, depth + 1);
    for (const auto& arg: ast->args) {
        if (arg) dumpExpression(arg, os, depth + 1);
    }
}

void dumpCastExpression(const CastExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "CastExpression\n";
    if (ast->operand) dumpExpression(ast->operand, os, depth + 1);
    if (ast->target_type) dumpTypeRef(ast->target_type, os, depth + 1);
}

void dumpStructExpression(const StructExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "StructExpression\n";
    dumpPathInExpression(ast->path, os, depth + 1);
    for (const auto& field: ast->fields) {
        dumpStructExprField(field, os, depth + 1);
    }
}

void dumpStructExprField(const StructExprField &ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "StructExprField name=" << ast.name << '\n';
    if (ast.value) dumpExpression(ast.value, os, depth + 1);
}

void dumpIfExpression(const IfExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "IfExpression\n";
    if (ast->condition) dumpExpression(ast->condition, os, depth + 1);
    if (ast->then_block) dumpBlockExpression(*ast->then_block, os, depth + 1);
    if (ast->else_branch) dumpExpression(ast->else_branch, os, depth + 1);
}

void dumpLoopExpression(const LoopExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "LoopExpression\n";
    if (ast->body) dumpBlockExpression(*ast->body, os, depth + 1);
}

void dumpWhileExpression(const WhileExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "WhileExpression\n";
    if (ast->condition) dumpExpression(ast->condition, os, depth + 1);
    if (ast->body) dumpBlockExpression(*ast->body, os, depth + 1);
}

void dumpBreakExpression(const BreakExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "BreakExpression\n";
    if (ast->expr) dumpExpression(ast->expr, os, depth + 1);
}

void dumpContinueExpression(const ContinueExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "ContinueExpression\n";
}

void dumpReturnExpression(const ReturnExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "ReturnExpression\n";
    if (ast->expr) dumpExpression(ast->expr, os, depth + 1);
}

void dumpGroupedExpression(const GroupedExpression *ast, std::ostream &os, int depth) {
    PRINT(os, depth) << "GroupedExpression\n";
    if (ast->expr) dumpExpression(ast->expr, os, depth + 1);
}

void dumpUnaryOperator(const UnaryOperator &op, std::ostream& os) {
    switch (op) {
        case UnaryOperator::Negation:
            os << '-'; break;
        case UnaryOperator::Not:
            os << '!'; break;
        case UnaryOperator::Dereference:
            os << '*'; break;
        case UnaryOperator::Borrow:
            os << '&'; break;
        case UnaryOperator::BorrowMut:
            os << "& mut"; break;
    }
}

void dumpBinaryOperator(const BinaryOperator &op, std::ostream &os) {
    switch (op) {
        case BinaryOperator::Add:
            os << '+'; break;
        case BinaryOperator::Subtract:
            os << '-'; break;
        case BinaryOperator::Multiply:
            os << '*'; break;
        case BinaryOperator::Divide:
            os << '/'; break;
        case BinaryOperator::Remainder:
            os << '%'; break;
        case BinaryOperator::BitwiseAnd:
            os << '&'; break;
        case BinaryOperator::BitwiseOr:
            os << '|'; break;
        case BinaryOperator::BitwiseXor:
            os << '^'; break;
        case BinaryOperator::ShiftLeft:
            os << "<<"; break;
        case BinaryOperator::ShiftRight:
            os << ">>"; break;
        case BinaryOperator::Equal:
            os << "=="; break;
        case BinaryOperator::NotEqual:
            os << "!="; break;
        case BinaryOperator::Greater:
            os << '>'; break;
        case BinaryOperator::Less:
            os << '<'; break;
        case BinaryOperator::GreaterEqual:
            os << ">="; break;
        case BinaryOperator::LessEqual:
            os << "<="; break;
        case BinaryOperator::LogicalAnd:
            os << "&&"; break;
        case BinaryOperator::LogicalOr:
            os << "||"; break;
    }
}

void dumpAssignmentOperator(const AssignmentOperator &op, std::ostream &os) {
    switch (op) {
        case AssignmentOperator::Assign:
            os << '='; break;
        case AssignmentOperator::AssignAdd:
            os << "+="; break;
        case AssignmentOperator::AssignSubtract:
            os << "-="; break;
        case AssignmentOperator::AssignMultiply:
            os << "*="; break;
        case AssignmentOperator::AssignDivide:
            os << "/="; break;
        case AssignmentOperator::AssignRemainder:
            os << "%="; break;
        case AssignmentOperator::AssignBitwiseAnd:
            os << "&="; break;
        case AssignmentOperator::AssignBitwiseOr:
            os << "|="; break;
        case AssignmentOperator::AssignBitwiseXor:
            os << "^="; break;
        case AssignmentOperator::AssignShiftLeft:
            os << "<<="; break;
        case AssignmentOperator::AssignShiftRight:
            os << ">>="; break;
    }
}

#undef PRINT
#undef INDENT

} // namespace ast