#include <uuid/uuid.h>

#include "../include/symbols.h"
#include "../include/interpreter.h"
#include "../include/node_manager.h"
#include "../include/codegen.h"


uuid_t SageCodeGenVisitor::build_store(uuid_t rhs, string variable_symbol) {
    // find where variable_symbol is stored
}

uuid_t SageCodeGenVisitor::build_return(uuid_t return_value) {
    // TODO: don't yet support multiple return values

    instruction move;
    if (return_value != NULL_ID) {
        SageSymbol* return_search = symbol_table.lookup(return_value);
        int register_ = return_search->volatile_register;

        if (return_search->is_variable) {
            auto info = analysis->variable_lifetimes[return_value];

            register_ = info.register_assignment;
            if (info.spilled) {
                // load the variable value into a volatile from the stack using the varlifetime offset
                auto load_from_stack = instruction(OP_LOAD, );
            }
        }

        move = instruction(OP_MOV, register_, 6);
        procedures[current_procedure.top()].push_back(move);
    }

    instruction return_isnt = instruction(OP_RET, 0);
    procedures[current_procedure.top()].push_back(return_inst);

    current_procedure.pop();
    vm->stack_frame.pop_stack_scope();

    return NULL_ID
}

uuid_t SageCodeGenVisitor::build_function_with_block(
    vector<string> argument_names, string function_name
) {
    bytecode code;

    uint32_t symbol_index = symbol_table.symbol_map[function_name];
    instruction procedure_label = instruction(OP_LABEL, symbol_index);
    code.push_back(procedure_label);

    procedures.push_back(code);
    current_procedure.push(procedures.size()-1);
    vm->stack_frame.push_stack_scope(function_name);

    return NULL_ID;
}

uuid_t SageCodeGenVisitor::build_alloca(SageType type, string var_name) {
}

uuid_t SageCodeGenVisitor::build_add(uuid_t value1, uuid_t value2) {
}

uuid_t SageCodeGenVisitor::build_sub(uuid_t value1, uuid_t value2) {
}

uuid_t SageCodeGenVisitor::build_mul(uuid_t value1, uuid_t value2) {
}

uuid_t SageCodeGenVisitor::build_div(uuid_t value1, uuid_t value2) {
}

uuid_t SageCodeGenVisitor::build_and(uuid_t value1, uuid_t value2) {
}

uuid_t SageCodeGenVisitor::build_or(uuid_t value1, uuid_t value2) {
}

uuid_t SageCodeGenVisitor::build_load(SageType type, string reference_name) {
}

uuid_t SageCodeGenVisitor::build_constant_int(int value) {
}

uuid_t SageCodeGenVisitor::build_constant_float(float value) {
}

uuid_t SageCodeGenVisitor::build_string_pointer(string value) {
}

uuid_t SageCodeGenVisitor::build_function_call(
    vector<uuid_t> args, string function_name
) {
}




























