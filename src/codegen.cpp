#include <unordered_map>
#include <memory>

#include "../include/symbols.h"
#include "../include/sage_types.h"
#include "../include/node_manager.h"
#include "../include/codegen.h"

using namespace llvm;
using namespace std;

SageCodeGenVisitor::SageCodeGenVisitor() {}

SageCodeGenVisitor::SageCodeGenVisitor(
    NodeManager* node_man,
    SageInterpreter* _vm
) {
    procedures.push_back(bytecode()); // push the global space into the bytecode builder
    current_procedure.push(0);
    
    symbol_table = SageSymbolTable();
    symbol_table.initialize(main_module.get(), *llvm_context);
    node_manager = node_man;
    vm = _vm;
    generate_uuid(NULL_ID);
}

llvm::Module* SageCodeGenVisitor::get_module() {
    return main_module.get();
}

// this is the first visitor called to compile a whole code module
uuid_t SageCodeGenVisitor::visit_program(NodeIndex node) {
    for (NodeIndex child : node_manager->get_children(node)) {
        switch(node_manager->get_nodetype(child)){
            case PN_FUNCDEF:
                if (node_manager->get_host_nodetype(child) != PN_BINARY) {
                    // ERROR!! CHILD SHOULD BE A BINARY NODE
                    return NULL_ID;
                }
                return visit_function_definition(child);

            case PN_FUNCDEC:
                if (node_manager->get_host_nodetype(child) != PN_BINARY) {
                    // ERROR!! CHILD SHOULD BE A BINARY NODE
                    return NULL_ID;
                }
                return visit_function_declaration(child);

            case PN_STRUCT:
                break;
            case PN_INCLUDE:
                break;
            case PN_RUN_DIRECTIVE:
                break;
            default:
                // ERROR
                break;
        }
    }

    // ERROR
    return NULL_ID;
}

uuid_t SageCodeGenVisitor::visit_function_return(uuid_t value) {
    symbol_table.current_function_has_returned = true;
    if (value != NULL_ID) {
        return build_return(value);
    }

    return build_return(NULL_ID);
}

uuid_t SageCodeGenVisitor::visit_function_declaration(NodeIndex node) {
    // binary node
    //  - unary     <-- function name
    //  - binary
    //      - block <-- function parameters
    //      - unary <-- function return type

    if (node_manager->get_host_nodetype(node_manager->get_right(node)) != PN_BINARY) {
        // ERROR!!
        return NULL_ID;
    }
    NodeIndex rightnode = node_manager->get_right(node);

    vector<SageType> parameter_types;
    vector<NodeIndex> parameters = node_manager->get_children(node_manager->get_left(rightnode));

    NodeIndex temp;
    for (NodeIndex parameter : parameters) {
        temp = node_manager->get_right(parameter);
        SageType ir_type = symbol_table.resolve_sage_type(node_manager, temp);
        parameter_types.push_back(ir_type);
    }

    SageType return_type = symbol_table.resolve_sage_type(node_manager, node_manager->get_right(rightnode));
    string function_name = node_manager->get_lexeme(node_manager->get_left(node));
    bool is_vararg = (node_manager->get_nodetype(parameters.at(parameters.size()-1)) == PN_VARARG);
    if (is_vararg) {
        parameter_types.pop_back();
    }

    auto function_type = SageFunctionType(return_type, parameter_types);

    // add to symbol table
    if (symbol_table.declare_symbol(function_name, function_type)) {
        // ERROR!!
        return NULL_ID;
    }

    return NULL_ID
}

uuid_t SageCodeGenVisitor::visit_function_definition(NodeIndex node) {
    symbol_table.push_scope();
    symbol_table.current_function_has_returned = false;

    // TODO: add function to symbol table
    NodeIndex trinary_node = node_manager->get_right(node);
    auto right_host = node_manager->get_host_nodetype(trinary_node);
    if (right_host != PN_TRINARY) {
        // ERROR!!
        return NULL_ID; 
    }
    
    vector<string> parameter_names;
    vector<SageType> parameter_types;
    vector<NodeIndex> parameters = node_manager->get_children(node_manager->get_left(trinary_node));
    for (NodeIndex parameter : parameters) {
        if (node_manager->get_host_nodetype(parameter) != PN_BINARY) {
            // ERROR!! SOMETHING IS WRONG PARAMETERS SHOULD NEVER BE STORED AS ANYTHING OTHER THAN BINARY NODES
            return NULL_ID;
        }

        auto right_side = node_manager->get_right(parameter);
        if (node_manager->get_host_nodetype(right_side) != PN_UNARY) {
            // ERROR!! TYPE CANNOT BE REPRESENTED BY ANYTHING OTHER THAN UNARY FOR NOW
            return NULL_ID;
        }
        auto param_type = symbol_table.resolve_sage_type(node_manager, right_side);

        // add to function scope
        string parameter_name = node_manager->get_lexeme(node_manager->get_left(parameter));
        parameter_names.push_back(parameter_name);
        bool already_exists = symbol_table.declare_symbol(parameter_name, param_type);
        if (already_exists) {
            // ERROR!!
            return NULL_ID;
        }

        parameter_types.push_back(param_type);
    }

    auto return_node = node_manager->get_middle(trinary_node);
    if (node_manager->get_host_nodetype(return_node) != PN_UNARY) {
        // ERROR!! TYPES ONLY REPRESENTED BY UNARY NODES RIGHT NOW
        return NULL_ID;
    }

    auto body_node = node_manager->get_right(trinary_node);
    if (node_manager->get_host_nodetype(body_node) != PN_BLOCK) {
        // ERROR!! RIGHT NODE OF FUNC DEF NODE SHOULD ONLY EVER BE A BLOCK NODE
        return NULL_ID;
    }

    auto function_type = SageFunctionType(return_type, parameter_types);

    if (symbol_table.declare_symbol(function_name, function_type)) {
        // ERROR!!!
        return NULL_ID;
    }

    build_function_with_block(
        parameter_names,
        function_name,
    );

    visit_codeblock(body_node);

    if (!symbol_table.current_function_has_returned) {
        if (return_type != VOID) {
            // ERROR: function was defined with no return statement
            symbol_table.pop_scope();
            return NULL_ID;
        }else {
            // auto return on void functions
            build_return(NULL_ID);
        }
    }

    symbol_table.pop_scope();
    return NULL_ID;
}

uuid_t SageCodeGenVisitor::visit_function_call(NodeIndex node) {
    NodeIndex args_node = node_manager->get_branch(node);
    string func_call_name = node_manager->get_lexeme(node);
    vector<uuid_t> args;

    // retrieve function from symbol table
    SageSymbol* symbol = symbol_table.lookup_symbol(func_call_name);
    if (symbol == nullptr) {
        // ERROR!!
        return NULL_ID;
    }

    for (NodeIndex arg : node_manager->get_children(args_node)) {
        // TODO:varargs breaks this kinda, fix later
        //if (index >= args_node->children.size()) {
        //    // ERROR!!
        //    return nullptr;
        //}

        // TODO: varargs breaks this kinda, fix later
        // auto irtype = symbol_table.derive_sage_type(unaryarg);
        //if (irtype != symbol->type->getParamType(index)) {
        //    // ERROR!!
        //    return;
        //}

        args.push_back(visit_unary_expr(arg));
    }

    return build_function_call(args, result_name);
}

uuid_t SageCodeGenVisitor::visit_codeblock(NodeIndex node) {
    for (NodeIndex child : node_manager->get_children(node)) {
        switch (node_manager->get_host_nodetype(child)) {
            case PN_UNARY:
                return visit_unary_expr(child);

            case PN_BINARY:
                return visit_binary_expr(child);

            case PN_TRINARY:
                return visit_trinary_expr(child);

            default:
                printf("unrecognized expression node: %s\n", nodetype_to_string(node_manager->get_nodetype(child)).c_str());
                break;
        }
    }

    return NULL_ID;
}

uuid_t SageCodeGenVisitor::visit_unary_expr(NodeIndex node) {
    SageSymbol* symbol_value;
    string node_lexeme = node_manager->get_lexeme(node);
    
    switch(node_manager->get_nodetype(node)) {
        case PN_VAR_REF: // PN_VAR_REF should do the same thing as PN_IDENTIFIER
        case PN_IDENTIFIER:
            symbol_value = symbol_table.lookup_symbol(node_lexeme);
            if (symbol_value == nullptr) {
                // ERROR!!
                return NULL_ID;
            }

            return build_load(symbol_value->type, node_lexeme);

        case PN_KEYWORD:
            if (node_manager->get_lexeme(node).contains("ret")) {
                return visit_function_return(
                    visit_expression(node_manager->get_branch(node))
                );
            }
            // ERROR: NOT IMPLEMENTED YET
            break;

        case PN_ELSE_BRANCH:
            break;

        case PN_VAR_DEC:
            return visit_variable_decl(node);

        case PN_STRUCT:
            // leaving this print statement here to see if we even actually need this case, it might never actually be called in practice
            printf("VISITING UNARY PN_STRUCT TYPE; UNIMPLEMENTED!!\n");
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
            // TODO: later
            break;
        default:
            break;
    }

    // ERROR
    return NULL_ID;
}

uuid_t SageCodeGenVisitor::visit_trinary_expr(NodeIndex node) {
    switch(node_manager->get_nodetype(node)) {
        case PN_FOR:
            break;
        case PN_ASSIGN:
            return visit_variable_decl(node);

        default:
            break;
    }
    
    return NULL_ID;
}

uuid_t SageCodeGenVisitor::visit_variable_assignment(NodeIndex node) {
    // NOTE: in the future to support heap memory reassignment we should maybe have something in the symbol table that indicates whether a value lives on the heap or not
    // so that we can inform this code section with what IR generation to use

    NodeIndex LHS = node_manager->get_left(node);
    SageSymbol* variable_symbol = symbol_table.lookup_symbol(node_manager->get_lexeme(LHS));
    if (variable_symbol == nullptr) {
        // ERROR!!
        return NULL_ID;
    }
    
    uuid_t RHS = visit_expression(node);
    
    return build_store(RHS, variable_symbol);
}

uuid_t SageCodeGenVisitor::visit_binary_expr(NodeIndex node) {
    switch(node_manager->get_nodetype(node)) {
        case PN_BINARY:
            return visit_expression(node);

        case PN_FUNCDEF:
            return visit_function_definition(node);

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
            break; // ERRRO!! COMPLEX REPRESENTED TYPES DONT EXIST YET
        default:
            break;
    }

    // ERROR
    return NULL_ID;
}

// TODO: split this into two functions trinary_decl and binary_decl
uuid_t SageCodeGenVisitor::visit_variable_decl(NodeIndex node) {
    auto concrete_node_type = node_manager->get_host_nodetype(node);
    if (concrete_node_type == PN_BINARY) {
        // left is variable identifier
        string variable_name = node_manager->get_lexeme(node_manager->get_left(node));
        
        // right is type identifier
        auto rightside = node_manager->get_right(node);
        if (node_manager->get_host_nodetype(rightside) != PN_UNARY) {
            // ERROR!! FOR NOW NON UNARY REPRESENTED TYPES DONT EXIST
            return NULL_ID;
        }
        auto type_ident = symbol_table.resolve_sage_type(node_manager, rightside);

        // update symbol table
        successful success = symbol_table.declare_symbol(variable_name, type_ident);
        if (!success) {
            // ERROR!!
            return NULL_ID;
        }

        return build_alloca(type_ident, variable_name);

    }else if (concrete_node_type == PN_TRINARY) {
        // left is variable identifier
        string variable_name = node_manager->get_lexeme(node_manager->get_left(node));
        
        // middle is type identifier
        auto middle = node_manager->get_middle(node);
        if (node_manager->get_host_nodetype(middle) != PN_UNARY) {
            // ERROR!! FOR NOW NON UNARY REPRESENTED TYPES DONT EXIST
            return NULL_ID;
        }
        auto type_ident = symbol_table.resolve_sage_type(node_manager, middle);

        // update symbol table
        if (symbol_table.lookup_symbol(variable_name) != nullptr) {
            // ERROR!! symbol already exists
            return NULL_ID;
        }

        // don't need to check the success of this call because we already know this symbol doesn't already exist
        symbol_table.declare_symbol(variable_name, type_ident);
        build_alloca(type_ident, variable_name);

        visit_expression(node_manager->get_right(node));

        return build_store(node_manager->get_right(node), variable_name);
    }

    // ERROR!!
    return NULL_ID;
}

uuid_t SageCodeGenVisitor::process_expression(NodeIndex node) {
    auto LHS_node = node_manager->get_left(node);
    auto RHS_node = node_manager->get_right(node);

    uuid_t LHS;
    uuid_t RHS;

    if (node_manager->get_host_nodetype(LHS_node) == PN_UNARY) {
        // TODO: make visit_unary_expr return VisitorValue again, this is the only visitor that should need to return something
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
            return build_add(LHS, RHS, "addtmp");

        case TT_SUB:
            return build_sub(LHS, RHS, "subtmp");

        case TT_MUL:
            return build_mul(LHS, RHS, "multmp");

        case TT_DIV:
            return build_div(LHS, RHS, "divtmp");

        case TT_AND:
            return build_and(LHS, RHS, "andtmp");

        case TT_OR:
            return build_or(LHS, RHS, "ortmp");

        case TT_BIT_AND: // TODO:
            break;
        case TT_BIT_OR: // TODO:
            break;
        default:
            break;
    }


    // ERROR!!
    return NULL_ID;
}

uuid_t SageCodeGenVisitor::visit_expression(NodeIndex node) {
    switch (node_manager->get_nodetype(node)) {
        case PN_BINARY:
            return process_expression(node);

        case PN_UNARY:
            return visit_unary_expr(node);

        case PN_RANGE:
            break; // TODO: later
 
        default:
            break;
    }
    
    // ERROR!!
    return NULL_ID;
}
