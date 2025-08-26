#include <unordered_map>
#include <memory>
#include <algorithm>

#include "../include/symbols.h"
#include "../include/sage_types.h"
#include "../include/node_manager.h"
#include "../include/codegen.h"
#include "../include/depgraph.h"

using namespace std;

ui32 SageCompiler::visit(NodeIndex node, DependencyGraph* dep_graph) {
    if (node_is_precompiled(node)) {
        return 0;
    }

    switch (node_manager->get_host_nodetype(node)) {
        case PN_UNARY:
            return visit_unary_expr(node, dep_graph);

        case PN_BINARY:
            return visit_binary_expr(node, dep_graph);

        case PN_TRINARY:
            return visit_trinary_expr(node, dep_graph);

        default:
            return 0;
    }
};

void SageCompiler::compile_exec_order(DependencyGraph* dep_graph) {
    if (dep_graph != nullptr) {
        auto ident_exec_order = dep_graph->get_exec_order();
        NodeIndex ast_node;
        for (int identifier : ident_exec_order) {
            ast_node = dep_graph->nodes[identifier].ast_pos;
            visit(ast_node, dep_graph);
            precompiled.insert(ast_node);
        }
    }
}

ui32 SageCompiler::visit_codeblock(NodeIndex node, DependencyGraph* dep_graph) {
    compile_exec_order(dep_graph);

    ui32 retval = -1;
    for (NodeIndex child : node_manager->get_children(node)) {
        if (node_is_precompiled(child)) {
            continue;
        }

        retval = visit(child, dep_graph);
    }

    return retval;
}

ui32 SageCompiler::visit_function_return(ui32 value) {
    symbol_table.current_function_has_returned = true;
    if (value != -1) {
        return build_return(value);
    }

    return build_return(-1);
}

ui32 SageCompiler::visit_function_declaration(NodeIndex node) {
    // binary node
    //  - unary     <-- function name
    //  - binary
    //      - block <-- function parameters
    //      - unary <-- function return type

    if (node_manager->get_host_nodetype(node_manager->get_right(node)) != PN_BINARY) {
        string message = str("Node: ", node, " was found to not be a BINARY node when visit_function_declaration requires it");
        logger.log_internal_error("codegen.cpp", 113, message);
        return -1;
    }
    NodeIndex rightnode = node_manager->get_right(node);

    vector<SageType*> parameter_types;
    const vector<NodeIndex>& parameters = node_manager->get_children(node_manager->get_left(rightnode));
    parameter_types.reserve(parameters.size());

    NodeIndex temp;
    SageType* ir_type;
    for (NodeIndex parameter : parameters) {
        temp = node_manager->get_right(parameter);
        ir_type = symbol_table.resolve_sage_type(node_manager, temp);
        parameter_types.push_back(ir_type);
    }

    SageType* return_type = symbol_table.resolve_sage_type(node_manager, node_manager->get_right(rightnode));
    vector<SageType*> return_types = {return_type};
    string function_name = node_manager->get_lexeme(node_manager->get_left(node));
    bool is_vararg = (node_manager->get_nodetype(parameters.at(parameters.size()-1)) == PN_VARARG);
    if (is_vararg) {
        parameter_types.pop_back();
    }

    auto function_type = TypeRegistery::get_function_type(return_types, parameter_types);

    // add to symbol table
    if (symbol_table.declare_symbol(function_name, function_type)) {
        string message = str("visit_function_declaration tried to declare a function symbol (", function_name,") that already existed.");
        logger.log_internal_error("codegen.cpp", 143, message);
        return -1;
    }

    return -1; 
}

ui32 SageCompiler::visit_function_definition(NodeIndex node) {
    symbol_table.push_scope();
    symbol_table.current_function_has_returned = false;

    // TODO: add function to symbol table
    NodeIndex trinary_node = node_manager->get_right(node);
    auto right_host = node_manager->get_host_nodetype(trinary_node);
    if (right_host != PN_TRINARY) {
        logger.log_internal_error("codegen.cpp", 158, str("visitor expected node (",node,") to be TRINARY, instead was: ", right_host));
        return -1; 
    }
    
    string function_name = node_manager->get_lexeme(node_manager->get_left(trinary_node));

    vector<string> parameter_names;
    vector<SageType*> parameter_types;
    const vector<NodeIndex>& parameters = node_manager->get_children(node_manager->get_left(trinary_node));
    parameter_types.reserve(parameters.size());
    parameter_names.reserve(parameters.size());
    string parameter_name;
    bool already_exists;
    NodeIndex right_side;
    SageType* param_type;
    for (NodeIndex parameter : parameters) {
        if (node_manager->get_host_nodetype(parameter) != PN_BINARY) {
            logger.log_internal_error("codegen.cpp", 175, str("expected param node to be BINARY"));
            return -1;
        }

        right_side = node_manager->get_right(parameter);
        if (node_manager->get_host_nodetype(right_side) != PN_UNARY) {
            logger.log_internal_error("codegen.cpp", 181, str("expected type node to be UNARY"));
            return -1;
        }
        param_type = symbol_table.resolve_sage_type(node_manager, right_side);

        // add to function scope
        parameter_name = node_manager->get_lexeme(node_manager->get_left(parameter));
        parameter_names.push_back(parameter_name);
        already_exists = symbol_table.declare_symbol(parameter_name, param_type);
        if (already_exists) {
            logger.log_internal_error("codegen.cpp", 191, str("tried to declare parameter that already existed: ", parameter_name));
            return -1;
        }

        parameter_types.push_back(param_type);
    }

    auto return_node = node_manager->get_middle(trinary_node);
    if (node_manager->get_host_nodetype(return_node) != PN_UNARY) {
        logger.log_internal_error("codegen.cpp", 200, str("expected type node to be UNARY"));
        return -1;
    }
    SageType* return_type = symbol_table.resolve_sage_type(node_manager, return_node);
    vector<SageType*> return_types(1);
    return_types.push_back(return_type);

    auto body_node = node_manager->get_right(trinary_node);
    if (node_manager->get_host_nodetype(body_node) != PN_BLOCK) {
        logger.log_internal_error("codegen.cpp", 209, str("right of FUNCDEF was not BLOCK"));
        return -1;
    }

    auto function_type = TypeRegistery::get_function_type(return_types, parameter_types);
    if (symbol_table.declare_symbol(function_name, function_type)) {
        logger.log_internal_error("codegen.cpp", 215, str("tried to declare function that already exists: ", function_name));
        return -1;
    }

    build_function_with_block(parameter_names, function_name);

    visit_codeblock(body_node);

    SageType* voidtype = TypeRegistery::get_builtin_type(VOID);
    if (!symbol_table.current_function_has_returned) {
        if (!return_type->match(voidtype)) {
            auto token = node_manager->get_token(node);
            logger.log_error(
                token,
                str("function (",function_name,") missing return statement"),
                GENERAL);
            symbol_table.pop_scope();
            return -1;
        }else {
            // auto return on void functions
            build_return(-1);
        }
    }

    symbol_table.pop_scope();
    return -1;
}

ui32 SageCompiler::visit_function_call(NodeIndex node) {
    NodeIndex args_node = node_manager->get_branch(node);
    string func_call_name = node_manager->get_lexeme(node);
    vector<ui32> args;

    // retrieve function from symbol table
    SageSymbol* symbol = symbol_table.lookup(func_call_name);
    if (symbol == nullptr) {
        logger.log_internal_error("codegen.cpp", 247, str(func_call_name, " was already defined"));
        return -1;
    }

    for (NodeIndex arg : node_manager->get_children(args_node)) {
        // TODO:varargs breaks this kinda, fix later
        //if (index >= args_node->children.size()) {
        //    return nullptr;
        //}

        // TODO: varargs breaks this kinda, fix later
        // auto irtype = symbol_table.derive_sage_type(unaryarg);
        //if (irtype != symbol->type->getParamType(index)) {
        //    return;
        //}

        args.push_back(visit_unary_expr(arg));
    }

    return build_function_call(args, func_call_name);
}

ui32 SageCompiler::visit_unary_expr(NodeIndex node, DependencyGraph* dep_graph) {
    SageSymbol* symbol_value;
    string node_lexeme = node_manager->get_lexeme(node);
    
    switch(node_manager->get_nodetype(node)) {
        case PN_VAR_REF: // PN_VAR_REF should do the same thing as PN_IDENTIFIER
        case PN_IDENTIFIER:
            symbol_value = symbol_table.lookup(node_lexeme);
            if (symbol_value == nullptr) {
                logger.log_internal_error("codegen.cpp", 281, str("symbol_value was nullptr"));
                return -1;
            }

            return build_load(symbol_value->type, node_lexeme);

        case PN_KEYWORD:
            if (node_manager->get_lexeme(node).find("ret") != std::string::npos) {
                return visit_function_return(
                    visit_expression(node_manager->get_branch(node))
                );
            }
            logger.log_internal_error("codegen.cpp", 293, str("keyword unimplemented: ", node_manager->get_lexeme(node)));
            break;

        case PN_ELSE_BRANCH:
            break;

        case PN_VAR_DEC:
            return visit_variable_decl(node);

        case PN_STRUCT:
            // leaving this print statement here to see if we even actually need this case, it might never actually be called in practice
            logger.log_internal_error("codegen.cpp", 304, str("unimplemented: visiting unary STRUCT type"));
            break;

        case PN_FUNCCALL:
            return visit_function_call(node);

        case PN_NUMBER:
            // NOTE: we don't actually know if this is supposed to be an int64, it could be something else, will have to see if this causes errors in testing
            return build_constant_int(stoi(node_lexeme));

        case PN_FLOAT:
            return build_constant_float(stof(node_lexeme));

        case PN_STRING:
            // TODO: probably need a function that wil process the raw string lexeme's into string format rules and strip away \"\"
            node_lexeme.erase(std::remove(node_lexeme.begin(), node_lexeme.end(), '"'), node_lexeme.end());
            return build_string_pointer(node_lexeme);

        case PN_LIST:
            logger.log_internal_error("codegen.cpp", 323, str("LIST type unimplemented"));
            break;

        case PN_RUN_DIRECTIVE: {
            if (node_is_precompiled(node)) {
                break;
            }
            auto block = node_manager->get_branch(node);
            return visit_codeblock(block, dep_graph);
        }

        default:
            break;
    }

    return -1;
}

ui32 SageCompiler::visit_trinary_expr(NodeIndex node, DependencyGraph* dep_graph) {
    ui32 retval = -1;
    switch(node_manager->get_nodetype(node)) {
        case PN_FOR:
            break;

        case PN_ASSIGN:
            retval = visit_variable_decl(node);
            break;

        case PN_FUNCDEF:
            retval = visit_function_definition(node);
            break;

        default:
            return -1;
    }
    
    return retval;
}

ui32 SageCompiler::visit_variable_assignment(NodeIndex node) {
    // NOTE: in the future to support heap memory reassignment we should maybe have something in the symbol table that indicates whether a value lives on the heap or not
    // so that we can inform this code section with what IR generation to use

    NodeIndex LHS = node_manager->get_left(node);
    SageSymbol* variable_symbol = symbol_table.lookup(node_manager->get_lexeme(LHS));
    if (variable_symbol == nullptr) {
        logger.log_internal_error("codegen.cpp", 361, str("variable_symbol was nullptr"));
        return -1;
    }

    NodeIndex RHS_idx = node_manager->get_right(node);
    ui32 RHS = visit_expression(RHS_idx);

    // check expression type against variable type
    SageSymbol* expression_symbol = symbol_table.lookup(RHS);
    if (expression_symbol == nullptr) {
        logger.log_internal_error("codegen.cpp", 371, str("unrecognized symbol: ", RHS_idx));
        return -1;
    }

    // auto variable_type = variable_symbol->type;
    // auto expression_type = expression_symbol->value.valuetype;

    return build_store(RHS, variable_symbol->identifier);
}

ui32 SageCompiler::visit_binary_expr(NodeIndex node, DependencyGraph* dep_graph) {
    switch(node_manager->get_nodetype(node)) {
        case PN_BINARY:
            return visit_expression(node);

        case PN_FUNCDEC:
            return visit_function_declaration(node);

        case PN_ASSIGN:
            // stack variable reassignment
            return visit_variable_assignment(node);

        case PN_VAR_DEC: 
            // allocating space on stack without assigning value
            return visit_variable_decl(node);

        case PN_VARARG:
            break;
        case PN_IF_BRANCH:
            break;
        case PN_WHILE:
            break;
        case PN_RANGE:
            break;
        case PN_TYPE:
            break;
        default:
            break;
    }

    return -1;
}

// TODO: split this into two functions trinary_decl and binary_decl
ui32 SageCompiler::visit_variable_decl(NodeIndex node) {
    auto concrete_node_type = node_manager->get_host_nodetype(node);
    if (concrete_node_type == PN_BINARY) {
        // left is variable identifier
        string variable_name = node_manager->get_lexeme(node_manager->get_left(node));
        
        // right is type identifier
        auto rightside = node_manager->get_right(node);
        if (node_manager->get_host_nodetype(rightside) != PN_UNARY) {
            logger.log_internal_error("codegen.cpp", 425, str("found type node that is not UNARY (expected to be UNARY)"));
            return -1;
        }
        auto type_ident = symbol_table.resolve_sage_type(node_manager, rightside);

        // update symbol table
        successful success = symbol_table.declare_symbol(variable_name, type_ident);
        if (!success) {
            logger.log_internal_error("codegen.cpp", 432, str("variable name already declared: ", variable_name));
            return -1;
        }

        return build_alloca(type_ident, variable_name);

    }
    if (concrete_node_type == PN_TRINARY) {
        // left is variable identifier
        string variable_name = node_manager->get_lexeme(node_manager->get_left(node));
        
        // middle is type identifier
        auto middle = node_manager->get_middle(node);
        if (node_manager->get_host_nodetype(middle) != PN_UNARY) {
            logger.log_internal_error("codegen.cpp", 446, str("found type node that is not UNARY (expected to be UNARY)"));
            return -1;
        }
        auto type_ident = symbol_table.resolve_sage_type(node_manager, middle);

        auto rightnode = node_manager->get_right(node);
        auto rightnode_token = node_manager->get_token(rightnode);
        ui32 rhs = visit_expression(rightnode);
        auto rhs_symbol = symbol_table.lookup(rhs);

        if (!rhs_symbol->type->match(type_ident)) {
            logger.log_error(
                rightnode_token,
                str(
                    variable_name,
                    "is of type",
                    type_ident->to_string(),
                    "but assigned expression was found to be",
                    rhs_symbol->type->to_string()),
                TYPE);
            return -1;
        }

        build_alloca(type_ident, variable_name);
        return build_store(rhs, variable_name);
    }

    return -1;
}

ui32 SageCompiler::process_expression(NodeIndex node) {
    auto LHS_node = node_manager->get_left(node);
    auto RHS_node = node_manager->get_right(node);

    ui32 LHS;
    ui32 RHS;

    if (node_manager->get_host_nodetype(LHS_node) == PN_UNARY) {
        LHS = visit_unary_expr(LHS_node);
    }else {
        LHS = process_expression(LHS_node);
    }

    if (node_manager->get_host_nodetype(RHS_node) == PN_UNARY) {
        RHS = visit_unary_expr(RHS_node);
    }else {
        RHS = process_expression(RHS_node);
    }
    
    // parse operator IR
    auto operator_tok = node_manager->get_token(node);
    
    // EQUALITY, LT, GT, GTE, LTE
    // ADD, SUB, MUL, DIV, EXP
    // AND, OR, BIT_AND, BIT_OR
    switch(operator_tok.token_type) {
        case TT_EQUALITY: // TODO:
            break;
        case TT_LT: // TODO:
            break;
        case TT_GT: // TODO:
            break;
        case TT_GTE: // TODO:
            break;
        case TT_LTE: // TODO:
            break;
        case TT_ADD:
            return build_add(LHS, RHS);

        case TT_SUB:
            return build_sub(LHS, RHS);

        case TT_MUL:
            return build_mul(LHS, RHS);

        case TT_DIV:
            return build_div(LHS, RHS);

        case TT_AND:
            return build_and(LHS, RHS);

        case TT_OR:
            return build_or(LHS, RHS);

        case TT_BIT_AND: // TODO:
            break;
        case TT_BIT_OR: // TODO:
            break;
        default:
            break;
    }

    logger.log_error(operator_tok, str(operator_tok.lexeme, " is not an operator."), SYNTAX);
    return -1;
}

ui32 SageCompiler::visit_expression(NodeIndex node) {
    switch (node_manager->get_host_nodetype(node)) {
        case PN_BINARY:
            return process_expression(node);

        case PN_UNARY:
            return visit_unary_expr(node);

        case PN_RANGE:
            break; // TODO: later
 
        default:
            break;
    }
    
    return -1;
}







