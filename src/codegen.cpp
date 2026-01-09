#include <memory>
#include <algorithm>

#include "../include/symbols.h"
#include "../include/sage_types.h"
#include "../include/node_manager.h"
#include "../include/codegen.h"

using namespace std;

// Hybrid symbol resolution - uses early-bound index when available, falls back to scope-based lookup
symbol_entry* SageCompiler::resolve_symbol(NodeIndex node) {
    // Fast path: check for early-bound resolved symbol
    int resolved = node_manager->get_resolved_symbol(node);
    if (resolved != -1) {
        return symbol_table.lookup_by_index(resolved);
    }
    
    // Lazy lookup using scope_id
    int scope_id = node_manager->get_scope_id(node);
    string name = node_manager->get_lexeme(node);
    int idx = symbol_table.lookup_from_scope(name, scope_id);
    
    if (idx != -1) {
        // Cache the resolved symbol for future lookups
        node_manager->set_resolved_symbol(node, idx);
        return symbol_table.lookup_by_index(idx);
    }
    
    return nullptr;
}

symbol_entry* SageCompiler::resolve_symbol_by_name(const string& name, int scope_id) {
    int idx = symbol_table.lookup_from_scope(name, scope_id);
    if (idx != -1) {
        return symbol_table.lookup_by_index(idx);
    }
    return nullptr;
}

ui32 SageCompiler::visit(NodeIndex node) {
    if (node_is_precompiled(node)) {
        return 0;
    }

    switch (node_manager->get_nodetype(node)) {
        case PN_BLOCK: {
            for (auto child : node_manager->get_children(node)) {
                visit(child);
            }
            return 0;
        }

        case PN_FUNCDEF:
        case PN_STRUCT:
        case PN_IF:
        case PN_WHILE:
        case PN_FOR:
        case PN_VAR_DEC:
        case PN_ASSIGN:
        case PN_RUN_DIRECTIVE:
        case PN_KEYWORD:
            return visit_statement(node);

        case PN_VAR_REF:
        case PN_NUMBER:
        case PN_STRING:
        case PN_FLOAT:
        case PN_FUNCCALL:
            return visit_expression(node);
        default:
            ErrorLogger::get().log_internal_error(
                "codegen.cpp",
                current_linenum,
                sen("Unhandled node type in SageCompiler::visit(NodeIndex):", node_manager->get_lexeme(node)));
            break;
    }
    return 0;
}

/*
 *
 * Statement Codegen
 *
 */

ui32 SageCompiler::visit_statement(NodeIndex node) {
    if (node_is_precompiled(node)) {
        return 0;
    }

    switch (node_manager->get_nodetype(node)) {
        case PN_FUNCDEF:
            return visit_funcdef(node);
        case PN_STRUCT:
            return visit_struct(node);
        case PN_IF:
            return visit_if(node);
        case PN_WHILE:
            return visit_while(node);
        case PN_FOR:
            return visit_for(node);
        case PN_VAR_DEC:
            return visit_vardec(node);
        case PN_ASSIGN:
            return visit_varassign(node);
        case PN_KEYWORD:
            return visit_keyword(node);
        case PN_RUN_DIRECTIVE: {
            if (!interpreter_mode) {
                return 0;
            }

            compile_dependency_resolution_order(node_manager->get_dependencies(node));
            auto blocknode = node_manager->get_branch(node);
            return visit(blocknode);
        }
        default:
            return visit_expression(node);
    }
}

ui32 SageCompiler::visit_keyword(NodeIndex node) {
    string lexeme = node_manager->get_lexeme(node);
    if (lexeme == "ret") {
        return visit_funcret(node);
    }
    if (lexeme == "continue") {
        return 0;
    }
    if (lexeme == "break") {
        return 0;
    }
    if (lexeme == "include") {
        return 0;
    }
    ErrorLogger::get().log_internal_error(
        "codegen.cpp",
        current_linenum,
        sen("found unrecognized keyword:", node_manager->get_lexeme(node)));
    return 0;
}

ui32 SageCompiler::visit_varassign(NodeIndex node) {
    // NOTE: in the future to support heap memory reassignment we should maybe have something in the symbol table that indicates whether a value lives on the heap or not
    // so that we can inform this code section with what IR generation to use

    NodeIndex LHS = node_manager->get_left(node);
    // Use hybrid symbol resolution - fast path if early-bound, else scope-based lookup
    auto variable_symbol = resolve_symbol(LHS);
    if (variable_symbol == nullptr) {
        logger.log_internal_error("codegen.cpp", current_linenum, str("variable_symbol was nullptr"));
        return -1;
    }

    NodeIndex RHS_idx = node_manager->get_right(node);
    ui32 RHS = visit_expression(RHS_idx);

    // check expression type against variable type
    auto expression_symbol = symbol_table.lookup_by_index(RHS);
    if (expression_symbol == nullptr) {
        logger.log_internal_error("codegen.cpp", current_linenum, str("unrecognized symbol: ", RHS_idx));
        return -1;
    }

    return build_store(RHS, variable_symbol->identifier);
}

ui32 SageCompiler::visit_funcdef(NodeIndex node) {
    string function_name = node_manager->get_lexeme(node_manager->get_left(node));
    symbol_table.function_visitor_state.push(function_visit(function_name));

    NodeIndex trinary_node = node_manager->get_right(node);
    auto right_host = node_manager->get_host_nodetype(trinary_node);
    if (right_host != PN_TRINARY) {
        logger.log_internal_error(
            "codegen.cpp",
            current_linenum,
            str("visitor expected node (",node,") to be TRINARY, instead was: ", right_host));
        return 0;
    }

    auto return_node = node_manager->get_middle(trinary_node);
    if (node_manager->get_host_nodetype(return_node) != PN_UNARY) {
        logger.log_internal_error("codegen.cpp", current_linenum, str("expected type node to be UNARY"));
        return 0;
    }
    SageType* return_type = symbol_table.resolve_sage_type(node_manager, return_node);
    vector<SageType*> return_types(1); // until we support more return types this is always length one
    return_types.push_back(return_type);

    build_function_with_block(function_name);
    auto body_node = node_manager->get_right(trinary_node);
    compile_dependency_resolution_order(node_manager->get_dependencies(node));
    visit(body_node);

    SageType* voidtype = TypeRegistery::get_builtin_type(VOID);
    if (!symbol_table.function_visitor_state.top().has_returned()) {
        if (!return_type->match(voidtype)) {
            auto token = node_manager->get_token(node);
            logger.log_error(
                token,
                str("function (",function_name,") missing return statement"),
                GENERAL);
            symbol_table.function_visitor_state.pop();
            return 0;
        }

        // auto return on void functions
        build_return(-1, function_name == "main");
    }

    symbol_table.function_visitor_state.pop();
    return 0;
}

ui32 SageCompiler::visit_struct(NodeIndex node) { return 0; }
ui32 SageCompiler::visit_if(NodeIndex node) { return 0; }
ui32 SageCompiler::visit_while(NodeIndex node) { return 0; }
ui32 SageCompiler::visit_for(NodeIndex node) { return 0; }

ui32 SageCompiler::visit_vardec(NodeIndex node) {
    auto concrete_node_type = node_manager->get_host_nodetype(node);
    if (concrete_node_type == PN_BINARY) {
        // left is variable identifier
        string variable_name = node_manager->get_lexeme(node_manager->get_left(node));
        return build_alloca(variable_name);
    }

    if (concrete_node_type == PN_TRINARY) {
        // left is variable identifier
        string variable_name = node_manager->get_lexeme(node_manager->get_left(node));

        auto rightnode = node_manager->get_right(node);
        ui32 rhs = visit_expression(rightnode);
        build_alloca(variable_name);
        return build_store(rhs, variable_name);
    }

    return 0;
}

ui32 SageCompiler::visit_funcret(NodeIndex node) {
    symbol_table.function_visitor_state.top().return_statement_count++;
    bool is_main = symbol_table.function_visitor_state.top().function_name == "main";

    auto branch_id = node_manager->get_branch(node);
    if (branch_id != NULL_INDEX) {
        return build_return(visit_expression(branch_id), is_main);
    }

    return build_return(-1, is_main);
}


/*
 *
 * Expression Codegen
 *
 */


ui32 SageCompiler::visit_expression(NodeIndex node) {
    if (node_is_precompiled(node)) {
        return 0;
    }

    if (node_manager->get_host_nodetype(node) == PN_BINARY) {
        return visit_binop(node);
    }

    return visit_literal(node);
}
ui32 SageCompiler::visit_varref(NodeIndex node) {
    auto node_lexeme = node_manager->get_lexeme(node);
    return build_load(node_lexeme);
}

// todo: might make more sense to change this func name to "visit_atom"
ui32 SageCompiler::visit_literal(NodeIndex node) {
    auto nodetype = node_manager->get_nodetype(node);
    string node_lexeme = node_manager->get_lexeme(node);
    switch (nodetype) {
        case PN_VAR_REF:
        case PN_IDENTIFIER:
            return visit_varref(node);
        case PN_FUNCCALL:
            return visit_funccall(node);
        case PN_STRING: {
            node_lexeme.erase(std::remove(node_lexeme.begin(), node_lexeme.end(), '"'), node_lexeme.end());
            return build_string_pointer(node_lexeme);
        }
        case PN_NUMBER:
            return build_constant_int(node_lexeme);
        case PN_FLOAT:
            return build_constant_float(node_lexeme);
        default:
            break;
    }
    return 0;
}
ui32 SageCompiler::visit_funccall(NodeIndex node) {
    string func_call_name = node_manager->get_lexeme(node);
    NodeIndex args_node = node_manager->get_branch(node);
    auto arg_children = node_manager->get_children(args_node);
    vector<ui32> args;
    args.reserve(arg_children.size());

    for (NodeIndex arg : arg_children) {
        args.push_back(visit_expression(arg));
    }

    return build_function_call(args, func_call_name);
}

ui32 SageCompiler::visit_binop(NodeIndex node) {
    auto token = node_manager->get_token(node);

    auto _build_add = [&](ui32 lhs, ui32 rhs) -> ui32 {return build_add(lhs, rhs);};
    auto _build_sub = [&](ui32 lhs, ui32 rhs) -> ui32 {return build_sub(lhs, rhs);};
    auto _build_div = [&](ui32 lhs, ui32 rhs) -> ui32 {return build_div(lhs, rhs);};
    auto _build_mul = [&](ui32 lhs, ui32 rhs) -> ui32 {return build_mul(lhs, rhs);};
    auto _build_and = [&](ui32 lhs, ui32 rhs) -> ui32 {return build_and(lhs, rhs);};
    auto _build_or = [&](ui32 lhs, ui32 rhs) -> ui32 {return build_or(lhs, rhs);};

    auto process_op = [&, this](function<ui32(ui32, ui32)> builder) -> ui32 {
        auto left = node_manager->get_left(node);
        ui32 lhs;
        if (node_manager->get_host_nodetype(left) == PN_BINARY) {
            lhs = visit_binop(left);
        }else {
            lhs = visit_literal(left);
        }

        auto right = node_manager->get_right(node);
        ui32 rhs;
        if (node_manager->get_host_nodetype(right) == PN_BINARY) {
            rhs = visit_binop(right);
        }else {
            rhs = visit_literal(right);
        }

        return builder(lhs, rhs);
    };

    switch (token.token_type) {
        case TT_ADD:
            return process_op(_build_add);
        case TT_SUB:
            return process_op(_build_sub);
        case TT_DIV:
            return process_op(_build_div);
        case TT_MUL:
            return process_op(_build_mul);
        case TT_AND:
            return process_op(_build_and);
        case TT_OR:
            return process_op(_build_or);
        case TT_GT:
            break;
        case TT_LT:
            break;
        case TT_GTE:
            break;
        case TT_LTE:
            break;
        case TT_BIT_OR:
            break;
        case TT_BIT_AND:
            break;
        case TT_EQUALITY:
            break;
        default:
            ErrorLogger::get().log_internal_error(
                "codegen.cpp",
                current_linenum,
                str("Node", node, "recieved incorrect node type for binary operation."));
            break;
    }
    return 0;
}

// TODO: ui32 SageCompiler::visit_unop(NodeIndex node) {}





