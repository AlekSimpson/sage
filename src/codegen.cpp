#include <memory>
#include <functional>

#include "../include/symbols.h"
#include "../include/node_manager.h"
#include "../include/codegen.h"

#include <complex>

using namespace std;

VisitorResultState VisitorResult::get_result_state(SageSymbolTable *table) {
    if (is_immediate()) return VisitorResultState::IMMEDIATE;
    symbol_entry *entry = table->lookup_by_index(symbol_table_index);
    if (entry->spilled) return VisitorResultState::SPILLED;
    if (entry->assigned_register != -1) return VisitorResultState::REGISTER;
    return VisitorResultState::VALUE;
};

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
    // NOTE: in the future to support heap memory reassignment we should maybe have something in the symbol table that indicates whether a value lives on the heap or not
    // so that we can inform this code section with what IR generation to use

    NodeIndex LHS = node_manager->get_left(node);
    // Use hybrid symbol resolution - fast path if early-bound, else scope-based lookup
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
    symbol_entry *function_entry = symbol_table.lookup(function_name, node_manager->get_scope_id(node));
    symbol_table.function_visitor_state.push(&function_entry->function_info);

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

    if (!symbol_table.function_visitor_state.top()->has_returned()) {
        // auto return on void functions
        bool is_main = function_name == "main";
        bool is_global = function_name == "global";
        bool is_final_program_return = symbol_table.program_uses_main_function ? is_main : is_global;

        SageOpCode opcode = OP_RET;
        if (is_final_program_return) {
            opcode = VOP_EXIT;
        }
        builder.build_instruction(opcode, 0, _00);
        builder.exit_frame();
    }

    symbol_table.function_visitor_state.pop();
    return VisitorResult();
}

VisitorResult SageCompiler::visit_struct(NodeIndex node) { return VisitorResult(); }
VisitorResult SageCompiler::visit_if(NodeIndex node) { return VisitorResult(); }
VisitorResult SageCompiler::visit_while(NodeIndex node) { return VisitorResult(); }
VisitorResult SageCompiler::visit_for(NodeIndex node) { return VisitorResult(); }

VisitorResult SageCompiler::visit_variable_definition(NodeIndex node) {
    auto concrete_node_type = node_manager->get_host_nodetype(node);
    if (concrete_node_type == PN_BINARY) {
        // left is variable identifier
        auto lhs = node_manager->get_left(node);
        string variable_name = node_manager->get_lexeme(lhs);
        symbol_entry *var_symbol = symbol_table.lookup(variable_name, node_manager->get_scope_id(lhs));

        return build_alloca(var_symbol);
    }

    if (concrete_node_type == PN_TRINARY) {
        // left is variable identifier
        auto lhs = node_manager->get_left(node);
        string variable_name = node_manager->get_lexeme(lhs);
        int scope_id = node_manager->get_scope_id(node);
        symbol_entry *var_symbol = symbol_table.lookup(variable_name, scope_id);

        auto rightnode = node_manager->get_right(node);
        auto rhs = visit_expression(rightnode);
        build_alloca(var_symbol);
        return build_store(rhs, var_symbol);
    }

    return VisitorResult();
}

VisitorResult SageCompiler::visit_function_return(NodeIndex node) {
    // TODO: doesn't yet support multiple return values
    symbol_table.function_visitor_state.top()->return_statement_count++;
    int function_symbol_index = symbol_table.function_visitor_state.top()->symbol_index;
    symbol_entry *function_entry = symbol_table.lookup_by_index(function_symbol_index);

    SageFunctionType *function_type = static_cast<SageFunctionType *>(function_entry->type);
    bool is_float_return = false;
    for (auto *type: function_type->return_type) {
        if (type->identify() == FLOAT) {
            is_float_return = true;
            break;
        }
    }

    bool is_main = function_entry->identifier == "main";
    bool is_global = function_entry->identifier == "global";
    bool is_program_exit = symbol_table.program_uses_main_function ? is_main : is_global;

    SageOpCode opcode = OP_RET;
    if (is_program_exit) {
        opcode = VOP_EXIT;
    }

    auto branch_id = node_manager->get_branch(node);
    if (branch_id != NULL_INDEX) {
        // store return value in return dedicated return registers
        VisitorResult return_value = visit_expression(branch_id);
        if (!symbol_table.needs_return_stack_pointer(function_entry->symbol_id)) {
            int return_register = function_entry->type->identify() == FLOAT ? 50 : 6;
            switch (return_value.get_result_state(&symbol_table)) {
                case VisitorResultState::IMMEDIATE: {
                    // note: is return value is an immediate then it must not be a float because floats literals are always spilled to the stack
                    builder.build_move_immediate(return_register, return_value.immediate_value);
                    break;
                }
                case VisitorResultState::SPILLED: {
                    symbol_entry *src_symbol = symbol_table.lookup_by_index(return_value.symbol_table_index);
                    if (is_float_return) {
                        builder.build_fload(return_register, src_symbol->spill_offset);
                    }else {
                        builder.build_load(return_register, src_symbol->spill_offset);
                    }
                    break;
                }
                case VisitorResultState::REGISTER: {
                    symbol_entry *src_symbol = symbol_table.lookup_by_index(return_value.symbol_table_index);
                    if (is_float_return) {
                        builder.build_fmove_register(return_register, src_symbol->assigned_register);
                    }else {
                        builder.build_move_register(return_register, src_symbol->assigned_register);
                    }
                    break;
                }
                case VisitorResultState::VALUE: {
                    symbol_entry *src_symbol = symbol_table.lookup_by_index(return_value.symbol_table_index);
                    int static_pointer = get_literal_static_pointer(src_symbol->symbol_id);
                    if (is_float_return) {
                        builder.build_fload(return_register, static_pointer);
                    }else {
                        builder.build_move_immediate(return_register, static_pointer);
                    }
                    break;
                }
            }
            builder.build_instruction(opcode, 0, _00);
            // FIX:
            builder.exit_frame();
            function_entry->assigned_register = return_register;
            return VisitorResult((table_index)function_entry->symbol_id);
        }

        // store return_value into stack return address
        switch (return_value.get_result_state(&symbol_table)) {
            case VisitorResultState::IMMEDIATE: {
                builder.build_instruction(OP_STORE, 6, return_value.immediate_value, _10);
                break;
            }
            case VisitorResultState::SPILLED: {
                symbol_entry *src_symbol = symbol_table.lookup_by_index(return_value.symbol_table_index);
                builder.build_instruction(OP_MEMCPY, 6, src_symbol->spill_offset, _10);
                break;
            }
            case VisitorResultState::REGISTER: {
                symbol_entry *src_symbol = symbol_table.lookup_by_index(return_value.symbol_table_index);
                builder.build_store_register(6, src_symbol->assigned_register);
                break;
            }
            case VisitorResultState::VALUE: {
                symbol_entry *src_symbol = symbol_table.lookup_by_index(return_value.symbol_table_index);
                int static_pointer = get_literal_static_pointer(src_symbol->symbol_id);
                builder.build_instruction(OP_MEMCPY, 6, static_pointer, _10);
                break;
            }
        }

        builder.build_instruction(opcode, 0, _00);
        // FIX:
        builder.exit_frame();
        function_entry->spilled = true;
        return VisitorResult((table_index)function_entry->symbol_id);
    }

    builder.build_instruction(opcode, 0, _00);
    // FIX: if a function has multiple return statements in it this will exit on the first one that is built and our frame logic will bug out
    builder.exit_frame();
    return VisitorResult();
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

VisitorResult SageCompiler::visit_literal(NodeIndex node) {
    auto nodetype = node_manager->get_nodetype(node);
    switch (nodetype) {
        case PN_LIST: {
            logger.log_internal_error_unsafe("codegen.cpp", current_linenum, "TODO: List literal visitor unimplemented.");
            return VisitorResult();
        }
        case PN_VAR_REF:
        case PN_IDENTIFIER:
            return build_load(node);
        case PN_FUNCCALL:
            return visit_function_call(node);
        case PN_STRING: {
            auto identifier = node_manager->get_identifier(node);
            table_index symbol_index = symbol_table.lookup_table_index(identifier, node_manager->get_scope_id(node));
            return VisitorResult(symbol_index);
        }
        case PN_CHARACTER_LITERAL: {
            auto identifier = node_manager->get_identifier(node);
            return VisitorResult(SageValue(identifier.c_str()[0]));
        }
        case PN_NUMBER: {
            return VisitorResult(stoi(node_manager->get_lexeme(node)));
        }
        case PN_FLOAT: {
            auto identifier = node_manager->get_identifier(node);
            table_index symbol_index = symbol_table.lookup_table_index(identifier, node_manager->get_scope_id(node));
            return VisitorResult(symbol_index);
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
        default:
            break;
    }
    return VisitorResult();
}

int SageCompiler::get_literal_static_pointer(table_index literal_symbol_table_index) {
    auto static_stack_pointer = symbol_table.entries.get(literal_symbol_table_index).static_stack_pointer;
    if (static_stack_pointer != -1) return static_stack_pointer;

    auto it = static_program_memory.find(literal_symbol_table_index);
    if (it != static_program_memory.end()) {
        int static_pointer = 0;
        for (int insertion_order_index = 0; insertion_order_index < (int) static_program_memory_insertion_order.size();
             ++insertion_order_index) {
            if (static_program_memory_insertion_order[insertion_order_index] == literal_symbol_table_index) {
                symbol_table.entries.get_pointer(literal_symbol_table_index)->static_stack_pointer = static_pointer;
                return static_pointer;
            }
            static_pointer += (int) static_program_memory[static_program_memory_insertion_order[insertion_order_index]].
                    size();
        }
    }
    return -1;
}

VisitorResult SageCompiler::visit_function_call(NodeIndex node) {
    NodeIndex args_node = node_manager->get_branch(node);
    auto arg_children = node_manager->get_children(args_node);
    vector<VisitorResult> args;
    args.reserve(arg_children.size());

    for (NodeIndex arg: arg_children) {
        args.push_back(visit_expression(arg));
    }

    auto identifier = node_manager->get_identifier(node);
    auto scoped_id = node_manager->get_scope_id(node);
    symbol_entry *function_symbol = symbol_table.lookup(identifier, scoped_id);
    string function_name = function_symbol->identifier;
    if (args.size() > 6) {
        logger.log_internal_error_unsafe(
            "builders.cpp",
            current_linenum,
            sen(function_name, "with more than 6 arguments is unimplemented."));
        return VisitorResult();
    }

    int argument_register_address = 0;
    for (VisitorResult arg_result: args) {
        //auto it = static_program_memory.find(arg_result.symbol_table_index);
        //if (it != static_program_memory.end()) {
        //    int static_pointer = 0;
        //    for (int order_index = 0; order_index < (int)static_program_memory_insertion_order.size(); ++order_index) {
        //        if (static_program_memory_insertion_order[order_index] == arg_result.symbol_table_index) {
        //            builder.build_move_immediate(argument_register_address, static_pointer);
        //            break;
        //        }
        //        static_pointer += (int)static_program_memory[static_program_memory_insertion_order[order_index]].size();
        //    }
        //    argument_register_address++;
        //    continue;
        //}
        int possible_literal_memory_pointer = get_literal_static_pointer(arg_result.symbol_table_index);
        if (possible_literal_memory_pointer != -1) {
            builder.build_move_immediate(argument_register_address, possible_literal_memory_pointer);
            argument_register_address++;
            continue;
        }


        auto arg_result_state = arg_result.get_result_state(&symbol_table);
        switch (arg_result_state) {
            case VisitorResultState::IMMEDIATE:
                builder.build_move_immediate(argument_register_address, arg_result.immediate_value);
                break;
            case VisitorResultState::SPILLED: {
                int temporary_register = get_volatile_register();
                symbol_entry *arg_entry = symbol_table.lookup_by_index(arg_result.symbol_table_index);
                builder.build_load(temporary_register, arg_entry->spill_offset);
                builder.build_move_register(argument_register_address, temporary_register);
                break;
            }
            case VisitorResultState::REGISTER: {
                symbol_entry *arg_entry = symbol_table.lookup_by_index(arg_result.symbol_table_index);
                builder.build_move_register(argument_register_address, arg_entry->assigned_register);
                break;
            }
            case VisitorResultState::VALUE: {
                symbol_entry *arg_entry = symbol_table.lookup_by_index(arg_result.symbol_table_index);
                builder.build_move_immediate(argument_register_address, arg_entry->value);
                break;
            }
        }
        argument_register_address++;
    }

    if (symbol_table.needs_return_stack_pointer(function_symbol->symbol_id)) {
        int pointer = symbol_table.function_visitor_state.top()->stack_return_pointer_counter;
        int return_bytesize = symbol_table.get_result_total_byte_size(symbol_table.function_visitor_state.top()->symbol_index);
        symbol_table.function_visitor_state.top()->stack_return_pointer_counter += return_bytesize;
        builder.build_move_immediate(6, pointer); // if the function return is on the stack then we don't need to use the function return register
    }

    builder.build_instruction(OP_CALL, get_procedure_frame_id(function_name), _00);

    return VisitorResult((table_index)function_symbol->symbol_id);
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

VisitorResult SageCompiler::visit_unary_operator(NodeIndex node) {
    return VisitorResult();
}
