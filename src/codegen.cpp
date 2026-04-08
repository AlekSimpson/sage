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

        case PN_STRUCT:
            return VisitorResult();
        case PN_FUNCDEF:
        case PN_IF:
        case PN_WHILE:
        case PN_FOR:
        case PN_VAR_DEC:
        case PN_ASSIGN:
        case PN_RUN_DIRECTIVE:
        case PN_KEYWORD:
            return visit_statement(node);

        default:
            return visit_expression(node);
    }
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
        return visit_return_statement(node);
    }
    if (lexeme == "continue") {
        return VisitorResult();
    }
    if (lexeme == "break") {
        return VisitorResult();
    }
    assertm(false, sen("found unrecognized keyword:", node_manager->get_lexeme(node)).data());
}

VisitorResult SageCompiler::visit_variable_assign(NodeIndex node) {
    NodeIndex LHS = node_manager->get_left(node);
    auto lhs_nodetype = node_manager->get_nodetype(LHS);
    if (lhs_nodetype == PN_FIELD_ACCESS) {
        auto field_access_result = visit_struct_field_access(LHS, 24, get_volatile_register(), nullptr, true, false);
        if (logger.has_errors()) {
            return VisitorResult();
        }

        NodeIndex right_node_index = node_manager->get_right(node);
        VisitorResult right_node_result = visit_expression(right_node_index);
        auto *right_result_type = right_node_result.result_type->expression_resolution_type();

        if (!field_access_result.result_type->match(right_result_type)) {
            Token token = node_manager->get_token(right_node_index);
            logger.log_error_unsafe(
                token,
                str("Cannot assign ", token.lexeme, " (type: ", right_result_type->to_string(), ") to type ",
                    field_access_result.result_type->to_string()),
                GENERAL
            );
            return VisitorResult();
        }

        bool lhs_is_array_element = node_manager->get_nodetype(node_manager->get_left(LHS)) == PN_ARRAY_ACCESS;
        right_node_result.to_stack_instruction_absolute(*this, field_access_result.temporary_result_register,
                                                        lhs_is_array_element, _10);
        return VisitorResult();
    }

    if (lhs_nodetype == PN_POINTER_DEREFERENCE) {
        // @ptr = value: load the address stored in ptr, then write RHS to that address
        auto branch_node = node_manager->get_branch(LHS);
        auto ptr_visit_result = visit_literal(branch_node);
        auto *symbol = symbol_table.lookup_by_index(ptr_visit_result.symbol_table_index);
        auto *ptr_type = dynamic_cast<SagePointerType *>(ptr_visit_result.result_type);
        int store_size = ptr_type->pointer_type->size;

        int addr_register = get_volatile_register();
        builder.build_load(addr_register, symbol->stack_offset, 8);

        NodeIndex right_node_index = node_manager->get_right(node);
        VisitorResult right_result = visit_expression(right_node_index);

        if (!ptr_type->pointer_type->match(right_result.result_type)) {
            Token token = node_manager->get_token(right_node_index);
            logger.log_error_unsafe(
                token,
                str("Cannot assign ", token.lexeme, " (type: ", right_result.result_type->to_string(), ") to type ",
                    ptr_type->pointer_type->to_string()),
                GENERAL
            );
            return VisitorResult();
        }

        auto [rhs_value, rhs_is_immediate] = right_result.materialize_register(*this);
        AddressMode address_mode = rhs_is_immediate ? _10 : _11;
        builder.build_instruction(OP_STOREA, store_size, addr_register, rhs_value, address_mode);
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
    assertm(variable_symbol != nullptr, str("variable_symbol was nullptr").data());
    assertm(variable_symbol->datatype != nullptr, str("variable symbol datatype was nullptr").data());

    NodeIndex right_node_index = node_manager->get_right(node);
    VisitorResult right_node_result = visit_expression(right_node_index);
    auto *right_expression_type = right_node_result.result_type->expression_resolution_type();

    if (!variable_symbol->datatype->match(right_expression_type)) {
        Token token = node_manager->get_token(right_node_index);
        logger.log_error_unsafe(
            token,
            str("Cannot assign ", token.lexeme, " (type: ", right_expression_type->to_string(), ") to type ",
                variable_symbol->datatype->to_string()),
            GENERAL
        );
        return VisitorResult();
    }

    return build_store(right_node_result, variable_symbol);
}

VisitorResult SageCompiler::visit_function_definition(NodeIndex node) {
    auto name_node = node_manager->get_left(node);
    string function_name = node_manager->get_lexeme(name_node);
    SymbolEntry *function_entry = symbol_table.lookup(function_name, node_manager->get_scope_id(node));
    SageFunctionType *function_type = (SageFunctionType *) function_entry->datatype;

    symbol_table.push_function_processing_context(function_entry->symbol_index);

    NodeIndex function_signature_node = node_manager->get_right(node);
    auto right_host = node_manager->get_host_nodetype(function_signature_node);
    assertm(right_host == PN_TRINARY,
            str("visitor expected node (", node, ") to be TRINARY, instead was: ", right_host).data());

    build_function_with_block(function_name);
    auto body_node = node_manager->get_right(function_signature_node);
    visit(body_node);

    // todo: also would need to check for if every control path in the function matches the return statement count
    assert(function_type->return_type.size() != 0);

    bool no_return_statement_found_when_one_was_expected = (
        !function_type->return_type[0]->match(TR::get_byte_type(VOID)) &&
        function_entry->processed_return_statement_count == 0);
    if (no_return_statement_found_when_one_was_expected) {
        Token token = node_manager->get_token(name_node);
        logger.log_error_unsafe(token, sen("found no return statements when at least one was expected."), GENERAL);
        return VisitorResult();
    }

    bool processed_no_return_statements = !(function_entry->processed_return_statement_count > 0);
    if (processed_no_return_statements) {
        // auto return on void functions
        bool is_main = function_name == "main";
        bool is_global = function_name == GLOBAL_NAME;
        bool is_program_exit = symbol_table.program_uses_main_function ? is_main : is_global;
        SageOpCode opcode = is_program_exit ? VOP_EXIT : OP_RET;
        builder.build_instruction(opcode, 0, _00);

        builder.exit_frame();

        symbol_table.pop_function_processing_context();
        return VisitorResult();
    }

    if (function_entry->processed_return_statement_count == function_entry->max_return_count) {
        builder.exit_frame();
    }

    symbol_table.pop_function_processing_context();
    return VisitorResult();
}

VisitorResult SageCompiler::visit_if(NodeIndex node) { return VisitorResult(); }
VisitorResult SageCompiler::visit_while(NodeIndex node) { return VisitorResult(); }
VisitorResult SageCompiler::visit_for(NodeIndex node) { return VisitorResult(); }

VisitorResult SageCompiler::visit_variable_definition(NodeIndex node) {
    auto concrete_node_type = node_manager->get_host_nodetype(node);
    assert(concrete_node_type == PN_BINARY);

    // left is variable identifier
    auto lhs = node_manager->get_left(node);
    string variable_name = node_manager->get_lexeme(lhs);
    SymbolEntry *var_symbol = symbol_table.lookup(variable_name, node_manager->get_scope_id(lhs));

    if (!var_symbol->spilled) return VisitorResult();

    auto datatype_identity = var_symbol->datatype->identify();
    if (datatype_identity != ARRAY && datatype_identity != DYN_ARRAY) {
        int offset = var_symbol->datatype->size;
        builder.build_instruction(OP_SUB, STACK_POINTER, STACK_POINTER, offset, _10);

        return VisitorResult();
    }

    int array_struct_start = get_volatile_register();
    int array_struct_length_address = get_volatile_register();
    int array_memory_start = get_volatile_register();
    builder.build_move_register(array_struct_start, STACK_POINTER);
    builder.build_move_register(array_struct_length_address, STACK_POINTER);
    builder.build_instruction(OP_SUB, array_struct_length_address, STACK_POINTER, 8, _10);

    int array_byte_size = ((SageArrayType *) var_symbol->datatype)->array_size;
    int array_length = ((SageArrayType *) var_symbol->datatype)->length;
    int offset = array_byte_size + var_symbol->datatype->size;
    // decrement SP first so array_memory_start captures new_SP (= first element address)
    builder.build_instruction(OP_SUB, STACK_POINTER, STACK_POINTER, offset, _10);
    builder.build_move_register(array_memory_start, STACK_POINTER);

    builder.build_instruction(OP_STOREA, 8, array_struct_start, array_memory_start, _11);
    builder.build_instruction(OP_STOREA, 8, array_struct_length_address, array_length, _10);

    return VisitorResult();
}

VisitorResult SageCompiler::visit_return_statement(NodeIndex node) {
    // TODO: doesn't yet support multiple return values
    // todo: when we do support multiple returns we need to check that the returned elements match the types in order and size
    symbol_table.function_being_processed().processed_return_statement_count++;
    int function_symbol_index = symbol_table.function_being_processed().symbol_index;
    SymbolEntry *function_entry = symbol_table.lookup_by_index(function_symbol_index);
    vector<SageType *> return_types = ((SageFunctionType *) function_entry->datatype)->return_type;

    bool is_main = function_entry->name == "main";
    bool is_global = function_entry->name == GLOBAL_NAME;
    bool is_program_exit = symbol_table.program_uses_main_function ? is_main : is_global;
    SageOpCode exit_opcode = is_program_exit ? VOP_EXIT : OP_RET;

    auto branch_id = node_manager->get_branch(node);
    if (branch_id == NULL_INDEX) {
        builder.build_instruction(exit_opcode, 0, _00);

        return VisitorResult();
    }

    // if there is a branch then that means we are returning something

    VisitorResult return_value = visit_expression(branch_id);

    auto expression_resolution_type = return_value.result_type->expression_resolution_type();
    if (!expression_resolution_type->match(return_types[0])) {
        Token token = node_manager->get_token(node);
        logger.log_error_unsafe(token, sen(function_entry->name, "expected", return_types[0]->to_string(),
                                           "type but found", expression_resolution_type->to_string(), "type instead."),
                                TYPE);
        return VisitorResult();
    }

    if (symbol_table.needs_return_stack_pointer(function_entry->symbol_index)) {
        return_value.to_stack_instruction_absolute(*this, 6, false, _10);
        // note: might need to set the ascending_memory bool on whether the func is returning a struct or array
        function_entry->spilled = true;
    } else {
        return_value.to_register_instruction(*this, 6, return_types[0]);
    }

    builder.build_instruction(exit_opcode, 0, _00);

    return VisitorResult(symbol_table, function_entry->symbol_index);
}

VisitorResult SageCompiler::visit_expression(NodeIndex node) {
    if (node_manager->get_host_nodetype(node) == PN_BINARY &&
        node_manager->get_nodetype(node) != PN_FIELD_ACCESS) {
        return visit_binary_operator(node);
    }

    return visit_literal(node);
}

VisitorResult SageCompiler::visit_struct_field_access(
    NodeIndex binary_access_node,
    int base_address_register,
    int offset_register,
    SageNamespace *current_namespace,
    bool struct_field_is_being_assigned_to,
    bool taking_address_of_field,
    bool absolute_addressing
) {
    int scope_id = node_manager->get_scope_id(binary_access_node);

    if (node_manager->get_host_nodetype(binary_access_node) != PN_BINARY) {
        // base case
        assert(current_namespace != nullptr);

        NodeIndex current_node = binary_access_node;
        auto current_nodetype = node_manager->get_nodetype(current_node);

        if (current_nodetype == PN_VAR_REF || current_nodetype == PN_IDENTIFIER) {
            string name = node_manager->get_identifier(current_node);
            SageType *type_entry;
            if (current_namespace->is_builtin()) {
                type_entry = ((BuiltinNamespace *) current_namespace)->get_field_type(name);
            } else {
                type_entry = symbol_table.lookup_by_index(current_namespace->fields[name])->datatype;
            }
            if (!current_namespace->is_field_member(name)) {
                // error
                Token token = node_manager->get_token(current_node);
                logger.log_error_unsafe(token, sen(token.lexeme, "is not a member of",
                                                   type_entry->get_base_type_string()), GENERAL);
                return VisitorResult();
            }

            int member_offset = current_namespace->is_builtin()
                                    ? ((BuiltinNamespace *) current_namespace)->get_field_offset(name)
                                    : current_namespace->get_field_offset(&symbol_table, name);
            int result_pointer_register = get_volatile_register();
            builder.build_instruction(OP_ADD, offset_register, offset_register, member_offset, _10);
            // Stack structs grow downward (base - offset); array element structs are stored
            // in increasing-address order via ADDR_MEMCPY, so their fields use base + offset.
            if (absolute_addressing) {
                builder.build_instruction(OP_ADD, result_pointer_register, base_address_register, offset_register, _11);
            } else {
                builder.build_instruction(OP_SUB, result_pointer_register, base_address_register, offset_register, _11);
            }

            if (struct_field_is_being_assigned_to || taking_address_of_field) {
                return VisitorResult(result_pointer_register, type_entry, true);
            }

            int value_result_register = get_volatile_register();
            builder.build_instruction(OP_LOADA, 8, value_result_register, result_pointer_register, _01);
            return VisitorResult(value_result_register, type_entry, true);
        }

        if (current_nodetype == PN_FUNCCALL) {
            int result_pointer_register = get_volatile_register();
            builder.build_instruction(OP_SUB, result_pointer_register, base_address_register, offset_register, _11);
            return visit_function_call(current_node, result_pointer_register);
        }

        Token token = node_manager->get_token(current_node);
        logger.log_error_unsafe(token, sen(token.lexeme, " is not callable."), GENERAL);
        return VisitorResult();
    }

    NodeIndex current_node = node_manager->get_left(binary_access_node);
    auto current_nodetype = node_manager->get_nodetype(current_node);

    if (current_nodetype == PN_ARRAY_ACCESS) {
        VisitorResult array_access_result = visit_array_access(current_node, base_address_register, offset_register,
                                                               current_namespace);
        if (logger.has_errors()) {
            return VisitorResult();
        }

        // check if there's more field access after this array access — includes both chained
        // PN_FIELD_ACCESS (e.g. arr[0].struct_member.field) and terminal identifiers (e.g. arr[0].length)
        NodeIndex right_node = node_manager->get_right(binary_access_node);
        auto right_node_type = node_manager->get_nodetype(right_node);
        bool has_more_field_access = right_node_type == PN_FIELD_ACCESS
                                     || right_node_type == PN_IDENTIFIER
                                     || right_node_type == PN_VAR_REF;

        if (has_more_field_access) {
            // error if trying to access field on non-struct type
            if (!array_access_result.result_type->is_struct()) {
                Token token = node_manager->get_token(current_node);
                logger.log_error_unsafe(token, sen("Cannot access field on non-struct type:",
                                                   array_access_result.result_type->to_string()), GENERAL);
                return VisitorResult();
            }

            // continue the chain with element as new base
            string type_name = array_access_result.result_type->to_string();
            SymbolEntry *type_entry = symbol_table.lookup(type_name, scope_id);

            int new_offset = get_volatile_register();
            builder.build_move_immediate(new_offset, 0);

            // absolute_addressing=true: element area grows upward (via ADDR_MEMCPY), so
            // fields within each element are accessed with ADD rather than SUB
            return visit_struct_field_access(
                right_node,
                array_access_result.temporary_result_register,
                new_offset,
                type_entry->type_namespace,
                struct_field_is_being_assigned_to,
                taking_address_of_field,
                true
            );
        }

        // terminal case - return element address or loaded value
        if (struct_field_is_being_assigned_to || taking_address_of_field) {
            return array_access_result;
        }

        int value_register = get_volatile_register();
        builder.build_instruction(OP_LOADA, array_access_result.result_type->size, value_register,
                                  array_access_result.temporary_result_register, _01);
        return VisitorResult(value_register, array_access_result.result_type, true);
    }

    if (current_nodetype == PN_VAR_REF || current_nodetype == PN_IDENTIFIER) {
        string name = node_manager->get_identifier(current_node);
        if (current_namespace == nullptr) {
            builder.build_move_immediate(offset_register, 0);

            auto *entry = symbol_table.lookup(name, scope_id);
            string type_string = entry->datatype->is_array()
                                     ? entry->datatype->to_string()
                                     : entry->datatype->get_base_type_string();
            auto *type_entry = symbol_table.lookup(type_string, scope_id);

            if (entry->datatype->is_pointer()) {
                builder.build_instruction(OP_ADD, offset_register, offset_register, entry->stack_offset, _10);
                int temporary_address = get_volatile_register();
                builder.build_instruction(OP_SUB, temporary_address, base_address_register, offset_register, _11);
                int new_base = get_volatile_register();
                builder.build_instruction(OP_LOADA, 8, new_base, temporary_address, _01);
                builder.build_move_immediate(offset_register, 0);

                string base_type_name = entry->datatype->get_base_type_string();
                auto *base_type = symbol_table.lookup(base_type_name, scope_id);

                return visit_struct_field_access(
                    node_manager->get_right(binary_access_node),
                    new_base,
                    offset_register,
                    base_type->type_namespace,
                    struct_field_is_being_assigned_to,
                    taking_address_of_field
                );
            }

            // String/struct parameters are passed by struct address in their assigned register
            // (r0, r1, ...) rather than being spilled into the callee's own frame.
            // Parameters have assigned_register != -1 (set by declare_parameter); local string
            // variables always have assigned_register == -1 because allocate_registers spills
            // them before reaching the register-assignment step.
            if (entry->assigned_register != -1 && entry->datatype->size > 8) {
                return visit_struct_field_access(
                    node_manager->get_right(binary_access_node),
                    entry->assigned_register,
                    offset_register,
                    type_entry->type_namespace,
                    struct_field_is_being_assigned_to,
                    taking_address_of_field
                );
            }

            builder.build_instruction(OP_ADD, offset_register, offset_register, entry->stack_offset, _10);
            return visit_struct_field_access(
                node_manager->get_right(binary_access_node),
                base_address_register,
                offset_register,
                type_entry->type_namespace,
                struct_field_is_being_assigned_to,
                taking_address_of_field
            );
        }

        auto *entry = symbol_table.lookup_by_index(current_namespace->fields[name]);
        auto *type_entry = symbol_table.lookup(entry->datatype->get_base_type_string(), scope_id);

        if (!current_namespace->is_field_member(name)) {
            // error
            Token token = node_manager->get_token(current_node);
            logger.log_error_unsafe(token, sen(token.lexeme, "is not a member of",
                                               type_entry->datatype->get_base_type_string()), GENERAL);
            return VisitorResult();
        }

        if (entry->datatype->is_pointer()) {
            builder.build_instruction(OP_ADD, offset_register, offset_register, entry->stack_offset, _10);
            int temporary_address = get_volatile_register();
            builder.build_instruction(OP_SUB, temporary_address, base_address_register, offset_register, _11);
            int new_base = get_volatile_register();
            builder.build_instruction(OP_LOADA, 8, new_base, temporary_address, _01);
            builder.build_move_immediate(offset_register, 0);

            string base_type_name = entry->datatype->get_base_type_string();
            auto *base_type = symbol_table.lookup(base_type_name, scope_id);

            return visit_struct_field_access(
                node_manager->get_right(binary_access_node),
                new_base,
                offset_register,
                base_type->type_namespace,
                struct_field_is_being_assigned_to,
                taking_address_of_field
            );
        }

        int member_offset = current_namespace->is_builtin()
                                ? ((BuiltinNamespace *) current_namespace)->get_field_offset(name)
                                : current_namespace->get_field_offset(&symbol_table, name);

        builder.build_instruction(OP_ADD, offset_register, offset_register, member_offset, _10);
        return visit_struct_field_access(
            node_manager->get_right(binary_access_node),
            base_address_register,
            offset_register,
            type_entry->type_namespace,
            struct_field_is_being_assigned_to,
            taking_address_of_field
        );
    }

    if (current_nodetype == PN_FUNCCALL) {
        int result_pointer_register = get_volatile_register();
        builder.build_instruction(OP_SUB, result_pointer_register, base_address_register, offset_register, _11);
        VisitorResult result = visit_function_call(current_node, result_pointer_register);

        // check if result is callable
        if (!result.result_type->is_callable() ||
            (result.result_type->is_pointer() &&
             !((SagePointerType *) result.result_type)->pointer_type->is_callable())
        ) {
            Token token = node_manager->get_token(current_node);
            logger.log_error_unsafe(token, sen(token.lexeme, "is not callable."), GENERAL);
            return VisitorResult();
        }

        builder.build_move_register(base_address_register, result.temporary_result_register);
        builder.build_move_immediate(offset_register, 0);

        auto *type_entry = symbol_table.lookup(result.result_type->to_string(), scope_id);

        return visit_struct_field_access(
            node_manager->get_right(binary_access_node),
            base_address_register,
            offset_register,
            type_entry->type_namespace,
            struct_field_is_being_assigned_to,
            taking_address_of_field
        );
    }

    Token token = node_manager->get_token(current_node);
    logger.log_error_unsafe(token, sen(token.lexeme, " is not callable."), GENERAL);
    return VisitorResult();
}

VisitorResult SageCompiler::visit_array_access(
    NodeIndex array_access_node,
    int base_address_register,
    int offset_register,
    SageNamespace *current_namespace
) {
    int scope_id = node_manager->get_scope_id(array_access_node);
    NodeIndex array_expr_node = node_manager->get_left(array_access_node);
    NodeIndex indices_block = node_manager->get_right(array_access_node);
    auto index_children = node_manager->get_children(indices_block);

    SageType *current_array_type = nullptr;
    int current_array_addr = get_volatile_register();

    auto array_nodetype = node_manager->get_nodetype(array_expr_node);
    if (array_nodetype == PN_VAR_REF || array_nodetype == PN_IDENTIFIER) {
        string name = node_manager->get_identifier(array_expr_node);

        if (current_namespace == nullptr) {
            // standalone array variable - look up directly
            SymbolEntry *entry = symbol_table.lookup(name, scope_id);
            current_array_type = entry->datatype;

            // calculate array struct address from stack
            builder.build_instruction(OP_SUB, current_array_addr, base_address_register, entry->stack_offset, _10);
        } else {
            // array is a field within a struct
            SymbolEntry *entry = symbol_table.lookup_by_index(current_namespace->fields[name]);
            current_array_type = entry->datatype;

            int field_offset = current_namespace->is_builtin()
                                   ? ((BuiltinNamespace *) current_namespace)->get_field_offset(name)
                                   : current_namespace->get_field_offset(&symbol_table, name);

            builder.build_instruction(OP_ADD, offset_register, offset_register, field_offset, _10);
            builder.build_instruction(OP_SUB, current_array_addr, base_address_register, offset_register, _11);
        }
    }
    assert(current_array_type != nullptr);

    int element_addr = get_volatile_register();
    SageType *element_type = nullptr;

    // iterate over all indices in the BLOCK
    for (size_t i = 0; i < index_children.size(); i++) {
        if (current_array_type->is_array()) {
            element_type = ((SageArrayType *) current_array_type)->array_type;
        } else if (current_array_type->identify() == DYN_ARRAY) {
            element_type = ((SageDynamicArrayType *) current_array_type)->array_type;
        }

        // load array.first pointer (offset 0 in array struct)
        int first_ptr_register = get_volatile_register();
        builder.build_instruction(OP_LOADA, 8, first_ptr_register, current_array_addr, _01);

        VisitorResult index_result = visit_expression(index_children[i]);
        auto [index_value, is_immediate] = index_result.materialize_register(*this);
        int index_register;
        if (is_immediate) {
            index_register = get_volatile_register();
            builder.build_move_immediate(index_register, index_value);
        } else {
            index_register = index_value;
        }

        // compute element address: first + (index * element_size)
        int element_size = element_type->size;
        int scaled_index = get_volatile_register();
        builder.build_instruction(OP_MUL, scaled_index, index_register, element_size, _10);
        builder.build_instruction(OP_ADD, element_addr, first_ptr_register, scaled_index, _11);

        // if more indices remain, load the sub-array struct address
        if (i < index_children.size() - 1) {
            builder.build_instruction(OP_LOADA, 8, current_array_addr, element_addr, _01);
            current_array_type = element_type;
        }
    }

    return VisitorResult(element_addr, element_type, true);
}

VisitorResult SageCompiler::visit_literal(NodeIndex node, bool taking_address_of_field) {
    auto nodetype = node_manager->get_nodetype(node);
    switch (nodetype) {
        case PN_VAR_REF:
        case PN_IDENTIFIER: {
            string reference_name = node_manager->get_identifier(node);
            int scope_id = node_manager->get_scope_id(node);
            auto symbol = symbol_table.lookup(reference_name, scope_id);
            return VisitorResult(symbol_table, symbol->symbol_index);
        }
        case PN_FUNCCALL:
            return visit_function_call(node);
        case PN_STRING:
        case PN_ARRAY_LITERAL: {
            auto identifier = node_manager->get_identifier(node);
            SymbolIndex symbol_index = symbol_table.lookup_table_index(identifier, node_manager->get_scope_id(node));
            return VisitorResult(symbol_table, symbol_index);
        }
        case PN_CHARACTER_LITERAL: {
            auto identifier = node_manager->get_identifier(node);
            SageValue value = SageValue(identifier.c_str()[0]);
            return VisitorResult(value);
        }
        case PN_NUMBER: {
            SageValue int_result = SageValue((int64_t) stoll(node_manager->get_lexeme(node)));
            return VisitorResult(int_result);
        }
        case PN_FLOAT: {
            SageValue float_result = SageValue((double) stof(node_manager->get_lexeme(node)));
            return VisitorResult(float_result);
        }
        case PN_BOOL: {
            auto identifier = node_manager->get_identifier(node);
            SageValue bool_result;
            if (identifier == "true") {
                bool_result = SageValue(true);
                return VisitorResult(bool_result);
            }
            if (identifier == "false") {
                bool_result = SageValue(false);
                return VisitorResult(bool_result);
            }
            assertm(false, sen("Expected to find 'true' or 'false', found", identifier).data());
        }
        case PN_FIELD_ACCESS:
            return visit_struct_field_access(node, 24, get_volatile_register(), nullptr, false,
                                             taking_address_of_field);
        case PN_POINTER_DEREFERENCE: {
            // get pointer variable of operand
            auto branch_node = node_manager->get_branch(node);
            auto visit_result = visit_literal(branch_node);
            return build_dereference_instructions(visit_result, node_manager->get_token(node));
        }
        case PN_POINTER_REFERENCE: {
            // move the full address of visit_result into a register
            auto branch = node_manager->get_branch(node);
            auto branch_nodetype = node_manager->get_nodetype(branch);
            Token token = node_manager->get_token(branch);
            if (branch_nodetype != PN_IDENTIFIER && branch_nodetype != PN_VAR_REF && branch_nodetype !=
                PN_FIELD_ACCESS) {
                logger.log_error_unsafe(token, sen(token.lexeme, "has no pointer."), GENERAL);
                return VisitorResult();
            }

            auto visit_result = visit_literal(branch, branch_nodetype == PN_FIELD_ACCESS);
            auto *symbol_entry = symbol_table.lookup_by_index(visit_result.symbol_table_index);
            assert(symbol_entry != nullptr);

            int dest_register = get_volatile_register();
            if (symbol_entry->static_pointer != -1) {
                builder.build_move_immediate(dest_register, symbol_entry->static_pointer);
                return VisitorResult(dest_register, TR::get_pointer_type(symbol_entry->datatype), true);
            }

            if (visit_result.state == VisitorResultState::SPILLED) {
                builder.build_instruction(OP_LOADR, dest_register, symbol_entry->stack_offset, _00);
                return VisitorResult(dest_register, TR::get_pointer_type(symbol_entry->datatype), true);
            }
            if (visit_result.state == VisitorResultState::TEMP_REGISTER) {
                // if its a temporary register this this MUST have been from referencing a struct field access
                symbol_entry->datatype = visit_result.result_type;
                builder.build_move_register(dest_register, visit_result.temporary_result_register);
                return VisitorResult(dest_register, TR::get_pointer_type(symbol_entry->datatype), true);
            }

            logger.log_error_unsafe(token, sen(token.lexeme, "cannot be referenced."), GENERAL);
            return VisitorResult();
        }
        case PN_NOT:
            break;
        default:
            break;
    }
    return VisitorResult();
}

VisitorResult SageCompiler::build_dereference_instructions(
    VisitorResult &operand_info,
    Token dereference_token
) {
    if (operand_info.result_type->identify() != POINTER) {
        logger.log_error_unsafe(
            dereference_token,
            sen("Cannot dereference non-pointer:", operand_info.result_type->to_string()),
            GENERAL
        );
        return VisitorResult();
    }

    int result_register = get_volatile_register();
    auto *result_pointer_type = dynamic_cast<SagePointerType *>(operand_info.result_type);
    int result_size = result_pointer_type->pointer_type->size;

    switch (operand_info.state) {
        case VisitorResultState::SPILLED: {
            auto *symbol = symbol_table.lookup_by_index(operand_info.symbol_table_index);
            builder.build_instruction(OP_LOADP, result_size, result_register, symbol->stack_offset, _00);
            break;
        }
        case VisitorResultState::TEMP_REGISTER: {
            builder.build_instruction(OP_LOADA, result_size, result_register, operand_info.temporary_result_register,
                                      _01);
            break;
        }
        case VisitorResultState::IMMEDIATE:
        case VisitorResultState::REGISTER:
        case VisitorResultState::VALUE:
        default: {
            logger.log_error_unsafe(
                dereference_token,
                str(dereference_token.lexeme, " cannot be dereferenced."),
                GENERAL);
        }
    }

    auto *result_type = result_pointer_type->pointer_type;
    return VisitorResult(result_register, result_type, true);
}

int SageCompiler::get_literal_static_pointer(SymbolIndex literal_symbol_table_index) {
    auto *target_entry = symbol_table.lookup_by_index(literal_symbol_table_index);
    return target_entry->static_pointer;
}

VisitorResult SageCompiler::visit_function_call(NodeIndex node, int first_parameter_pointer_register) {
    // TODO: more than 6 function parameters not supported
    // TODO: multiple return values not supported
    NodeIndex args_node = node_manager->get_branch(node);
    auto arg_children = node_manager->get_children(args_node);

    // Look up the function symbol before evaluating args so we can inspect parameter types
    // and choose the correct evaluation strategy per argument.
    auto function_name = node_manager->get_identifier(node);
    auto scoped_id = node_manager->get_scope_id(node);
    SymbolEntry *function_symbol = symbol_table.lookup(function_name, scoped_id);
    if (function_symbol == nullptr) {
        Token token = node_manager->get_token(node);
        logger.log_error_unsafe(token, sen("Call to undefined function: ", token.lexeme), GENERAL);
        return VisitorResult();
    }

    vector<SageType *> &defined_parameter_types = ((SageFunctionType *) function_symbol->datatype)->parameter_types;

    // When first_parameter_pointer_register is set, it occupies parameter slot 0 implicitly,
    // so explicit arg_children[i] maps to defined_parameter_types[i + 1].
    int param_offset = (first_parameter_pointer_register != -1) ? 1 : 0;

    vector<VisitorResult> args;
    args.reserve(arg_children.size());
    for (int i = 0; i < (int) arg_children.size(); i++) {
        NodeIndex arg = arg_children[i];
        int param_idx = i + param_offset;
        SageType *param_type = (param_idx < (int) defined_parameter_types.size())
                                   ? defined_parameter_types[param_idx]
                                   : nullptr;

        // For string parameters passed as a field access (e.g. p.name), evaluate to produce
        // the struct address instead of loading the buffer pointer from the first field.
        // The callee uses the struct address to access both .bytes and .length.
        if (param_type && param_type->match(TR::get_string_type()) &&
            node_manager->get_nodetype(arg) == PN_FIELD_ACCESS) {
            args.push_back(visit_literal(arg, true));
        } else {
            args.push_back(visit_expression(arg));
        }
    }

    assertm(args.size() <= 6, sen(function_name, "with more than 6 arguments is unimplemented.").data());

    int argument_register_address = 0;
    if (first_parameter_pointer_register != -1) {
        builder.build_move_register(argument_register_address, first_parameter_pointer_register);
        argument_register_address++;
    }

    for (VisitorResult arg_result: args) {
        int possible_literal_memory_pointer = get_literal_static_pointer(arg_result.symbol_table_index);
        if (possible_literal_memory_pointer != -1) {
            builder.build_move_immediate(argument_register_address, possible_literal_memory_pointer);
            argument_register_address++;
            continue;
        }

        if (!arg_result.result_type->expression_resolution_type()->match(defined_parameter_types[argument_register_address])) {
            Token token = node_manager->get_token(node);
            logger.log_error_unsafe(token, sen(function_name, "function parameter type was expected to be",
                                               defined_parameter_types[argument_register_address]->to_string(),
                                               "but found ", arg_result.result_type->expression_resolution_type()->to_string(), "type instead."), TYPE);
            return VisitorResult();
        }

        arg_result.to_register_instruction(
            *this, argument_register_address, defined_parameter_types[argument_register_address]);

        argument_register_address++;
    }

    if (symbol_table.needs_return_stack_pointer(function_symbol->symbol_index)) {
        auto *return_type = dynamic_cast<SageFunctionType *>(function_symbol->datatype)->return_type[0];

        if (return_type->identify() == ARRAY || return_type->identify() == DYN_ARRAY) {
            // For arrays: allocate struct + elements, initialize struct
            int array_struct_address = get_volatile_register();
            int array_element_address = get_volatile_register();

            builder.build_move_register(array_struct_address, STACK_POINTER);

            SageArrayType *array_type = (SageArrayType *) return_type;
            int total_size = return_type->size + array_type->array_size; // struct + elements

            builder.build_instruction(OP_SUB, STACK_POINTER, STACK_POINTER, total_size, _10);
            builder.build_move_register(array_element_address, STACK_POINTER);

            // Initialize struct: store element pointer and length
            builder.build_instruction(OP_STOREA, 8, array_struct_address, array_element_address, _11);
            int length_address = get_volatile_register();
            builder.build_instruction(OP_SUB, length_address, array_struct_address, 8, _10);
            builder.build_instruction(OP_STOREA, 8, length_address, array_type->length, _10);

            builder.build_move_register(6, array_struct_address);
        } else {
            // For structs: just allocate space
            int return_bytesize = symbol_table.get_result_total_byte_size(function_symbol->symbol_index);
            builder.build_move_register(6, STACK_POINTER);
            builder.build_instruction(OP_SUB, STACK_POINTER, STACK_POINTER, return_bytesize, _10);
        }
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
        case TT_STAR:
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
        case TT_FIELD_ACCESSOR:
        case TT_LBRACKET:
            return visit_struct_field_access(node, 24, get_volatile_register(), nullptr, false, false);
        default:
            assertm(false, sen("Node", node, "recieved incorrect node type for binary operation.").data());
    }
    return VisitorResult();
}
