#include <memory>
#include <functional>
#include <cassert>

#include "../include/symbols.h"
#include "../include/node_manager.h"
#include "../include/codegen.h"

using namespace std;

VisitorResult SageCompiler::visit(NodeIndex node) {
    switch (node_manager->get_nodetype(node)) {
        case PN_BLOCK: {
            for (auto child: node_manager->get_children(node)) {
                visit(child);
            }
            return VisitorResult();
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
            ErrorLogger::get().log_internal_error_unsafe(
                "codegen.cpp",
                current_linenum,
                sen("Unhandled node type in SageCompiler::visit(NodeIndex):", node_manager->get_lexeme(node)));
            break;
    }
    return VisitorResult();
}

/*
 *
 * Statement Codegen
 *
 */

VisitorResult SageCompiler::visit_statement(NodeIndex node) {
    switch (node_manager->get_nodetype(node)) {
        case PN_FUNCDEF:
            return visit_function_definition(node);
        case PN_STRUCT:
            return visit_struct(node);
        case PN_IF:
            return visit_if(node);
        case PN_WHILE:
            return visit_while(node);
        case PN_FOR:
            return visit_for(node);
        case PN_VAR_DEC:
            return visit_variable_definition(node);
        case PN_ASSIGN:
            return visit_variable_assign(node);
        case PN_KEYWORD:
            return visit_keyword(node);
        case PN_RUN_DIRECTIVE: {
            if (!generating_compile_time_bytecode()) {
                return VisitorResult();
            }

            auto blocknode = node_manager->get_branch(node);
            return visit(blocknode);
        }
        default:
            return visit_expression(node);
    }
}

VisitorResult SageCompiler::visit_keyword(NodeIndex node) {
    string lexeme = node_manager->get_lexeme(node);
    if (lexeme == "ret") {
        return visit_function_return(node);
    }
    if (lexeme == "continue") {
        return VisitorResult();
    }
    if (lexeme == "break") {
        return VisitorResult();
    }
    ErrorLogger::get().log_internal_error_unsafe(
        "codegen.cpp",
        current_linenum,
        sen("found unrecognized keyword:", node_manager->get_lexeme(node)));
    return VisitorResult();
}

VisitorResult SageCompiler::visit_variable_assign(NodeIndex node) {
    NodeIndex LHS = node_manager->get_left(node);
    auto lhs_nodetype = node_manager->get_nodetype(LHS);
    if (lhs_nodetype == PN_FIELD_ACCESS) {
        auto field_access_result = visit_struct_field_access(LHS);

        NodeIndex right_node_index = node_manager->get_right(node);
        VisitorResult right_node_result = visit_expression(right_node_index);
        right_node_result.to_stack_instruction(*this, field_access_result.temporary_result_register, _10);
        return VisitorResult();
    }

    if (lhs_nodetype != PN_IDENTIFIER && lhs_nodetype != PN_VAR_REF) {
        Token token = node_manager->get_token(node);
        logger.log_error_unsafe(token, sen("Can only assign values to structure members or to variables."), GENERAL);
        return VisitorResult();
    }

    auto lhs_identifier = node_manager->get_identifier(LHS);
    auto scope_id = node_manager->get_scope_id(LHS);
    auto variable_symbol = symbol_table.lookup(lhs_identifier, scope_id);
    if (variable_symbol == nullptr) {
        // should be an assert
        logger.log_internal_error_unsafe("codegen.cpp", current_linenum, str("variable_symbol was nullptr"));
        return VisitorResult();
    }

    NodeIndex right_node_index = node_manager->get_right(node);
    VisitorResult right_node_result = visit_expression(right_node_index);
    return build_store(right_node_result, variable_symbol);
}

VisitorResult SageCompiler::visit_function_definition(NodeIndex node) {
    string function_name = node_manager->get_lexeme(node_manager->get_left(node));
    SymbolEntry *function_entry = symbol_table.lookup(function_name, node_manager->get_scope_id(node));
    symbol_table.push_function_processing_context(function_entry->symbol_index);

    NodeIndex function_signature_node = node_manager->get_right(node);
    auto right_host = node_manager->get_host_nodetype(function_signature_node);
    if (right_host != PN_TRINARY) {
        // TODO: checks like this really should be asserts that aren't generated in production mode
        logger.log_internal_error_unsafe(
            "codegen.cpp",
            current_linenum,
            str("visitor expected node (", node, ") to be TRINARY, instead was: ", right_host));
        return VisitorResult();
    }

    build_function_with_block(function_name);
    auto body_node = node_manager->get_right(function_signature_node);
    visit(body_node);

    if (!symbol_table.function_being_processed().has_returned()) {
        // auto return on void functions
        bool is_main = function_name == "main";
        bool is_global = function_name == GLOBAL_NAME;
        bool is_final_program_return = symbol_table.program_uses_main_function ? is_main : is_global;

        SageOpCode opcode = OP_RET;
        if (is_final_program_return) {
            opcode = VOP_EXIT;
        }
        builder.build_instruction(opcode, 0, _00);
        builder.exit_frame();
    }

    symbol_table.pop_function_processing_context();
    return VisitorResult();
}

VisitorResult SageCompiler::visit_struct(NodeIndex node) {
    return VisitorResult();
}

VisitorResult SageCompiler::visit_if(NodeIndex node) { return VisitorResult(); }
VisitorResult SageCompiler::visit_while(NodeIndex node) { return VisitorResult(); }
VisitorResult SageCompiler::visit_for(NodeIndex node) { return VisitorResult(); }

VisitorResult SageCompiler::visit_variable_definition(NodeIndex node) {
    auto concrete_node_type = node_manager->get_host_nodetype(node);
    if (concrete_node_type == PN_BINARY) {
        // left is variable identifier
        auto lhs = node_manager->get_left(node);
        string variable_name = node_manager->get_lexeme(lhs);
        SymbolEntry *var_symbol = symbol_table.lookup(variable_name, node_manager->get_scope_id(lhs));

        // TODO: also need to generate bytecode to write the variable SageValue default_value to the variables given register or stack position
        //       will probably need to make some sort of .as_register() functino for SageValue for the cases where the variable is a register

        return build_alloca(var_symbol);
    }

    if (concrete_node_type == PN_TRINARY) {
        // left is variable identifier
        auto lhs = node_manager->get_left(node);
        string variable_name = node_manager->get_lexeme(lhs);
        int scope_id = node_manager->get_scope_id(node);
        SymbolEntry *var_symbol = symbol_table.lookup(variable_name, scope_id);

        auto rightnode = node_manager->get_right(node);
        build_alloca(var_symbol);
        auto rhs = visit_expression(rightnode);

        // TODO: also need to check here if the rhs was equal to "--" then we should not generate bytecode to auto initialize the variable
        // TODO: also need to generate bytecode to write the variable SageValue default_value to the variables given register or stack position
        //       will probably need to make some sort of .as_register() functino for SageValue for the cases where the variable is a register

        return build_store(rhs, var_symbol);
    }

    return VisitorResult();
}

VisitorResult SageCompiler::visit_function_return(NodeIndex node) {
    // TODO: doesn't yet support multiple return values
    symbol_table.function_being_processed().return_statement_count++;
    int function_symbol_index = symbol_table.function_being_processed().symbol_index;
    SymbolEntry *function_entry = symbol_table.lookup_by_index(function_symbol_index);
    vector<SageType *> return_types = ((SageFunctionType *) function_entry->datatype)->return_type;

    bool is_main = function_entry->name == "main";
    bool is_global = function_entry->name == GLOBAL_NAME;
    bool is_program_exit = symbol_table.program_uses_main_function ? is_main : is_global;

    SageOpCode opcode = OP_RET;
    auto branch_id = node_manager->get_branch(node);
    if (branch_id == NULL_INDEX) {
        builder.build_instruction(opcode, 0, _00);
        if (is_program_exit) {
            builder.build_instruction(VOP_EXIT, 0, _00);
        }
        if (function_entry->return_statement_count == function_entry->max_return_count) {
            builder.exit_frame();
        }
        return VisitorResult();
    }


    VisitorResult return_value = visit_expression(branch_id);
    if (symbol_table.needs_return_stack_pointer(function_entry->symbol_index)) {
        return_value.to_stack_instruction(*this, 6, _10);
        function_entry->spilled = true;
    } else {
        return_value.to_register_instruction(*this, 6, return_types[0]);
    }

    builder.build_instruction(opcode, 0, _00);
    if (is_program_exit) {
        builder.build_instruction(VOP_EXIT, 0, _00);
    }
    if (function_entry->return_statement_count == function_entry->max_return_count) {
        builder.exit_frame();
    }
    return VisitorResult(symbol_table, function_entry->symbol_index);
}


/*
 *
 * Expression Codegen
 *
 */


VisitorResult SageCompiler::visit_expression(NodeIndex node) {
    if (node_manager->get_host_nodetype(node) == PN_BINARY) {
        return visit_binary_operator(node);
    }

    return visit_literal(node);
}

VisitorResult SageCompiler::visit_struct_field_access(NodeIndex node) {
    assert(node_manager->get_nodetype(node) == PN_BINARY);

    int final_stack_offset_register = get_volatile_register();
    SageNamespace *current_namespace = nullptr;
    NodeIndex access_operator_node = node;
    NodeIndex leftmost_node = node_manager->get_left(access_operator_node);
    int scope_id = node_manager->get_scope_id(leftmost_node);
    if (node_manager->get_nodetype(leftmost_node) == PN_IDENTIFIER || node_manager->get_nodetype(leftmost_node) ==
        PN_VAR_REF) {
        string name = node_manager->get_identifier(leftmost_node);
        auto *entry = symbol_table.lookup(name, scope_id);
        auto *type_entry = symbol_table.lookup(entry->datatype->get_base_type_string(), scope_id);
        builder.build_move_immediate(final_stack_offset_register, entry->stack_offset);
        current_namespace = type_entry->get_namespace();
    } else if (node_manager->get_nodetype(leftmost_node) == PN_FUNCCALL) {
        VisitorResult result = visit_function_call(leftmost_node);
        builder.build_move_register(final_stack_offset_register, result.temporary_result_register);
        auto *type_entry = symbol_table.lookup(result.result_type->get_base_type_string(), scope_id);
        current_namespace = type_entry->get_namespace();
    } else {
        auto token = node_manager->get_token(leftmost_node);
        logger.log_error_unsafe(token, str(token.lexeme, "is not callable."), GENERAL);
        return VisitorResult();
    }

    access_operator_node = node_manager->get_right(access_operator_node);
    FieldAccessTreeIterator iterator = FieldAccessTreeIterator(access_operator_node, node_manager);
    leftmost_node = iterator.next();

    string left_name;
    SageType *left_type;
    while (leftmost_node != NULL_INDEX) {
        left_name = node_manager->get_identifier(leftmost_node);

        auto *entry = symbol_table.lookup(left_name, scope_id);
        left_type = entry->datatype;
        if (current_namespace->is_field_member(left_name, left_type) && left_type->identify() != POINTER) {
            builder.build_instruction(OP_ADD, final_stack_offset_register,
                                      final_stack_offset_register,
                                      current_namespace->get_member_offset(left_name, left_type), _00);
        } else if (current_namespace->is_field_member(left_name, left_type) && left_type->identify() == POINTER) {
            builder.build_instruction(OP_ADD, final_stack_offset_register,
                                      final_stack_offset_register,
                                      current_namespace->get_member_offset(left_name, left_type), _00);

            // auto dereference struct members that are pointers
            VisitorResult access_result = VisitorResult(final_stack_offset_register, left_type, true);
            VisitorResult deref_result = build_dereference_instructions(access_result, node_manager->get_token(leftmost_node));
            builder.build_move_register(final_stack_offset_register, deref_result.temporary_result_register);
        } else if (current_namespace->is_method(left_name, left_type) && iterator.has_next()) {
            // return value should be new stack base offset
            auto call_result = visit_function_call(leftmost_node, final_stack_offset_register);
            if (!call_result.result_type->is_callable()) {
                auto token = node_manager->get_token(leftmost_node);
                logger.log_error_unsafe(token, str("Return value of function call ", token.lexeme, " is not callable."),
                                        GENERAL);
                return VisitorResult();
            }

            if (call_result.result_type->identify() == POINTER) {
                call_result = build_dereference_instructions(call_result, node_manager->get_token(leftmost_node));
            }

            builder.build_move_register(final_stack_offset_register, call_result.temporary_result_register);
        } else if (current_namespace->is_method(left_name, left_type)) {
            // return normal function call visitor result
            return visit_function_call(leftmost_node, final_stack_offset_register);
        } else {
            auto token = node_manager->get_token(leftmost_node);
            logger.log_error_unsafe(token, str("Cannot find struct member, ", token.lexeme, "."), GENERAL);
            return VisitorResult();
        }

        auto *type_entry = symbol_table.lookup(left_type->get_base_type_string(), scope_id);
        current_namespace = type_entry->get_namespace();
        leftmost_node = iterator.next();
    }

    return VisitorResult(final_stack_offset_register, left_type, true);
}

VisitorResult SageCompiler::visit_literal(NodeIndex node) {
    auto nodetype = node_manager->get_nodetype(node);
    switch (nodetype) {
        case PN_LIST: {
            logger.log_internal_error_unsafe("codegen.cpp", current_linenum,
                                             "TODO: List literal visitor unimplemented.");
            return VisitorResult();
        }
        case PN_VAR_REF:
        case PN_IDENTIFIER: {
            string reference_name = node_manager->get_identifier(node);
            int scope_id = node_manager->get_scope_id(node);
            auto symbol = symbol_table.lookup(reference_name, scope_id);
            return VisitorResult(symbol_table, symbol->symbol_index);
        }
        case PN_FUNCCALL:
            return visit_function_call(node);
        case PN_STRING: {
            auto identifier = node_manager->get_identifier(node);
            SymbolIndex symbol_index = symbol_table.lookup_table_index(identifier, node_manager->get_scope_id(node));
            return VisitorResult(symbol_table, symbol_index);
        }
        case PN_CHARACTER_LITERAL: {
            auto identifier = node_manager->get_identifier(node);
            return VisitorResult(SageValue(identifier.c_str()[0]));
        }
        case PN_NUMBER: {
            return VisitorResult(stoi(node_manager->get_lexeme(node)));
        }
        case PN_FLOAT: {
            return VisitorResult(stof(node_manager->get_lexeme(node)));
        }
        case PN_BOOL: {
            auto identifier = node_manager->get_identifier(node);
            if (identifier == "true") {
                return VisitorResult(SageValue(true));
            }
            if (identifier == "false") {
                return VisitorResult(SageValue(false));
            }
            logger.log_internal_error_unsafe("codegen.cpp", current_linenum,
                                             sen("Expected to find 'true' or 'false', found", identifier));
            return VisitorResult();
        }
        case PN_FIELD_ACCESS: {
            return visit_struct_field_access(node);
        }
        case PN_POINTER_DEREFERENCE: {
            // get pointer variable of operand
            auto visit_result = visit_literal(node_manager->get_branch(node));
            return build_dereference_instructions(visit_result, node_manager->get_token(node));
        }
        case PN_POINTER_REFERENCE: {
            // move the full address of visit_result into a register
            auto visit_result = visit_literal(node_manager->get_branch(node));
            return VisitorResult();
        }
        case PN_NOT:
            break;
        default:
            break;
    }
    return VisitorResult();
}

VisitorResult SageCompiler::build_dereference_instructions(VisitorResult &operand_info, Token dereference_token) {
    if (operand_info.result_type->identify() != POINTER) {
        logger.log_error_unsafe(
            dereference_token,
            sen("Cannot dereference non-pointer type:", operand_info.result_type->to_string()),
            SEMANTIC
        );
        return VisitorResult();
    }

    int result_register = get_volatile_register();

    switch (operand_info.state) {
        case VisitorResultState::SPILLED: {
            auto *symbol = symbol_table.lookup_by_index(operand_info.symbol_table_index);
            builder.build_load(result_register, symbol->stack_offset, symbol->datatype->size);
            break;
        }
        case VisitorResultState::TEMP_REGISTER: {
            builder.build_instruction(OP_LOAD, operand_info.result_type->size, result_register, operand_info.temporary_result_register, _01);
            break;
        }
        case VisitorResultState::IMMEDIATE:
        case VisitorResultState::REGISTER:
        case VisitorResultState::VALUE:
        case VisitorResultState::LIST:
        default: {
            // error
            logger.log_error_unsafe(
                dereference_token,
                str(dereference_token.lexeme, " cannot be dereferenced."),
                SEMANTIC);
        }
    }

    auto *result_type = static_cast<SagePointerType *>(operand_info.result_type)->pointer_type;
    return VisitorResult(result_register, result_type, true);
}

int SageCompiler::get_literal_static_pointer(SymbolIndex literal_symbol_table_index) {
    auto *target_entry = symbol_table.lookup_by_index(literal_symbol_table_index);
    auto static_stack_pointer = target_entry->static_stack_pointer;
    if (static_stack_pointer != -1) return static_stack_pointer;

    int bytes_passed = 0;
    SymbolEntry *current_entry;
    for (auto index: static_program_memory_insertion_order) {
        current_entry = symbol_table.lookup_by_index(index);
        if (index == literal_symbol_table_index) {
            target_entry->static_stack_pointer = bytes_passed;
            return bytes_passed;
        }

        bytes_passed += current_entry->datatype->size;
    }
    return -1;
}

VisitorResult SageCompiler::visit_function_call(NodeIndex node, int first_parameter_pointer_register) {
    // TODO: more than 6 function parameters not supported
    // TODO: multiple return values not supported
    NodeIndex args_node = node_manager->get_branch(node);
    auto arg_children = node_manager->get_children(args_node);
    vector<VisitorResult> args;
    args.reserve(arg_children.size());

    for (NodeIndex arg: arg_children) {
        args.push_back(visit_expression(arg));
    }

    auto identifier = node_manager->get_identifier(node);
    auto scoped_id = node_manager->get_scope_id(node);
    SymbolEntry *function_symbol = symbol_table.lookup(identifier, scoped_id);
    string function_name = function_symbol->name;
    if (args.size() > 6) {
        logger.log_internal_error_unsafe(
            "builders.cpp",
            current_linenum,
            sen(function_name, "with more than 6 arguments is unimplemented."));
        return VisitorResult();
    }

    int argument_register_address = 0;
    if (first_parameter_pointer_register != -1) {
        builder.build_move_register(argument_register_address, first_parameter_pointer_register);
        argument_register_address++;
    }

    vector<SageType *> &defined_parameter_types = ((SageFunctionType *) function_symbol->datatype)->parameter_types;
    for (VisitorResult arg_result: args) {
        int possible_literal_memory_pointer = get_literal_static_pointer(arg_result.symbol_table_index);
        if (possible_literal_memory_pointer != -1) {
            builder.build_move_immediate(argument_register_address, possible_literal_memory_pointer);
            argument_register_address++;
            continue;
        }

        arg_result.to_register_instruction(
            *this, argument_register_address, defined_parameter_types[argument_register_address]);

        argument_register_address++;
    }

    if (symbol_table.needs_return_stack_pointer(function_symbol->symbol_index)) {
        int pointer = symbol_table.function_being_processed().stack_return_pointer_counter;
        int return_bytesize = symbol_table.get_result_total_byte_size(
            symbol_table.function_being_processed().symbol_index);
        symbol_table.function_being_processed().stack_return_pointer_counter += return_bytesize;
        builder.build_move_immediate(6, pointer);
        // if the function return is on the stack then we don't need to use the function return register
        function_symbol->spilled = true;
    } else {
        function_symbol->spilled = false;
        function_symbol->assigned_register = 6;
    }

    builder.build_instruction(OP_CALL, get_procedure_frame_id(function_name), _00);

    auto *return_type = dynamic_cast<SageFunctionType *>(function_symbol->datatype)->return_type[0];
    return VisitorResult(6, return_type, function_symbol->symbol_index, true);
}

VisitorResult SageCompiler::visit_binary_operator(NodeIndex node) {
    auto token = node_manager->get_token(node);

    auto _build_add = [&](VisitorResult lhs, VisitorResult rhs) -> VisitorResult { return build_add(lhs, rhs); };
    auto _build_sub = [&](VisitorResult lhs, VisitorResult rhs) -> VisitorResult { return build_sub(lhs, rhs); };
    auto _build_div = [&](VisitorResult lhs, VisitorResult rhs) -> VisitorResult { return build_div(lhs, rhs); };
    auto _build_mul = [&](VisitorResult lhs, VisitorResult rhs) -> VisitorResult { return build_mul(lhs, rhs); };
    auto _build_and = [&](VisitorResult lhs, VisitorResult rhs) -> VisitorResult { return build_and(lhs, rhs); };
    auto _build_or = [&](VisitorResult lhs, VisitorResult rhs) -> VisitorResult { return build_or(lhs, rhs); };

    auto process_operator = [&, this](function<VisitorResult(VisitorResult, VisitorResult)> _builder) -> VisitorResult {
        auto left = node_manager->get_left(node);
        VisitorResult lhs;
        if (node_manager->get_host_nodetype(left) == PN_BINARY) {
            lhs = visit_binary_operator(left);
        } else {
            lhs = visit_literal(left);
        }

        auto right = node_manager->get_right(node);
        VisitorResult rhs;
        if (node_manager->get_host_nodetype(right) == PN_BINARY) {
            rhs = visit_binary_operator(right);
        } else {
            rhs = visit_literal(right);
        }

        return _builder(lhs, rhs);
    };

    switch (token.token_type) {
        case TT_ADD:
            return process_operator(_build_add);
        case TT_SUB:
            return process_operator(_build_sub);
        case TT_DIV:
            return process_operator(_build_div);
        case TT_MUL:
            return process_operator(_build_mul);
        case TT_AND:
            return process_operator(_build_and);
        case TT_OR:
            return process_operator(_build_or);
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
            ErrorLogger::get().log_internal_error_unsafe(
                "codegen.cpp",
                current_linenum,
                str("Node", node, "recieved incorrect node type for binary operation."));
            break;
    }
    return VisitorResult();
}
