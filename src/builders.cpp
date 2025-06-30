#include "../include/symbols.h"
#include "../include/interpreter.h"
#include "../include/node_manager.h"
#include "../include/codegen.h"

ui32 SageCodeGenVisitor::build_store(ui32 rhs, string variable_symbol) {
    // find where variable_symbol is stored
    auto lifetime_info = analysis->variable_lifetimes[variable_symbol];
    SageSymbol* expression_symbol = symbol_table.lookup(rhs);

    // TODO: put checks to see if this value even needs to be stored in the heap
    int expression_pointer = vm->store_in_heap(expression_symbol->get_value());

    if (lifetime_info.spilled) {
        // if variable is spilled then store rhs into the stack
        add_instruction(OP_STORE, expression_pointer, lifetime_info.spill_offset, {0, 0, 0, 0});
        return -1;

    }

    // if the variable has a register assignment then move rhs into that register
    add_instruction(OP_MOV, expression_pointer, lifetime_info.variable_assignment, {0, 0, 0, 0});

    return -1;
}

ui32 SageCodeGenVisitor::build_return(ui32 return_value) {
    // TODO: doesn't yet support multiple return values
    // this is why we are just by default loading the 
    // return value into sr6 because sr6 is the first 
    // return register.

    if (return_value == -1) {
        // there is no value to return
        add_instruction(OP_RET, 0);
        current_procedure.pop();
        vm->stack_frame.pop_stack_scope();
        return -1;
    }

    // otherwise there is a return value
    SageSymbol* return_search = symbol_table.lookup(return_value);

    // assigned upon symbol creation in the visitor (if it is a temp value)
    int register_ = return_search->volatile_register; 

    if (return_search->is_variable) {
        auto info = analysis->variable_lifetimes[return_value];

        if (info.spilled) {
            // load the variable value into a volatile from the stack using the varlifetime offset
            add_instruction(OP_LOAD, 6, info.spill_offset, {0, 0, 0, 0});
            add_instruction(OP_RET, 0);

            current_procedure.pop();
            vm->stack_frame.pop_stack_scope();

            return -1;
        }

        // otherwise the register is not spilled the value should be located at the assigned register
        register_ = info.register_assignment;

        // move the value into the function return register
        add_instruction(OP_MOV, register_, 6, {1, 0, 0, 0});
        add_instruction(OP_RET, 0);

        current_procedure.pop();
        vm->stack_frame.pop_stack_scope();

        return -1
    }

    // temp values need the sage value moved into a volatile register
    auto sagevalue = return_search->value;
    int heap_pointer = vm->store_in_heap(sagevalue);
    add_instruction(OP_MOV, heap_pointer, 6, {0, 0, 0, 0});
    add_instruction(OP_RET, 0);

    current_procedure.pop();
    vm->stack_frame.pop_stack_scope();

    return -1;
}

ui32 SageCodeGenVisitor::build_function_with_block(
    vector<string> argument_names, string function_name
) {
    vm->procedure_label_encoding[function_name] = procedure_encoding;
    vm->procedure_encoding++;

    bytecode code;

    uint32_t symbol_index = symbol_table.symbol_map[function_name];
    instruction procedure_label = instruction(OP_LABEL, symbol_index);
    code.push_back(procedure_label);

    procedures.push_back(code);
    current_procedure.push(procedures.size()-1);
    vm->stack_frame.push_stack_scope(function_name);

    return -1;
}

ui32 SageCodeGenVisitor::build_alloca(SageType type, string var_name) {
    // NOTE: honestly not sure if we need the type variable or not but I think we may need it in the future
    VariableLifetime lifetime_info = analysis->variable_lifetimes[var_name];

    if (lifetime_info.spilled) {
        // then we can do normal stack stuff
        add_instruction(OP_ADD, STACK_POINTER, STACK_POINTER, 1, {0, 1, 0, 0});
    }

    // otherwise its actually stored in a register and we dont' need to do anything
    return -1;
}

int process_operand(SageCodeGenVisitor* visitor, ui32 value) {
    auto symbol = symbol_table.lookup(value);
    int result_value;
    if (symbol->is_variable) {
        // if its a variable then 
        // find out where variable is stored
        VariableLifetime info = analysis->variable_lifetimes[symbol->identifier];

        if (!vm->volatile_is_stale(symbol->volatile_register)) {
            // if the variable has already been loaded into a volatile register that hasn't gone stale
            // then just use that register
            return symbol->volatile_register;
        }

        if (info.spilled) {
            // load from stack into volatile
            result_value = vm->volatile_register();
            add_instruction(OP_LOAD, result_value, info.spill_offset, {0, 0, 0, 0});
        }
    }else {
        result_value = symbol->volatile_register;
    }

    return result_value;
}

ui32 SageCodeGenVisitor::build_operator(ui32 value1, ui32 value2, SageOpCode opcode) {
    // we need to check if the values are variables or temp expression values
    int value1 = process_operand(this, value1);
    int value2 = process_operand(this, value2);

    int result_volatile = vm->volatile_register();
    add_instruction(opcode, result_volatile, value1, value2, {0, 1, 1, 0});

    ui32 result_symbol_id = symbol_table.declare_internal_symbol(result_volatile);
    if (result_symbol_id == -1) {
        // ERROR??
        return -1;
    }

    return result_symbol_id;
}

ui32 SageCodeGenVisitor::build_add(ui32 value1, ui32 value2) {
    return build_operator(value1, value2, OP_ADD);
}

ui32 SageCodeGenVisitor::build_sub(ui32 value1, ui32 value2) {
    return build_operator(value1, value2, OP_SUB);
}

ui32 SageCodeGenVisitor::build_mul(ui32 value1, ui32 value2) {
    return build_operator(value1, value2, OP_MUL);
}

ui32 SageCodeGenVisitor::build_div(ui32 value1, ui32 value2) {
    return build_operator(value1, value2, OP_DIV);
}

ui32 SageCodeGenVisitor::build_and(ui32 value1, ui32 value2) {
    return build_operator(value1, value2, OP_AND);
}

ui32 SageCodeGenVisitor::build_or(ui32 value1, ui32 value2) {
    return build_operator(value1, value2, OP_OR);
}

ui32 SageCodeGenVisitor::build_load(SageType type, string reference_name) {
    VariableLifetime info = analysis->variable_lifetimes[reference_name];
    SageSymbol* symbol = symbol_table.lookup(reference_name);

    if (!vm->volatile_is_stale(symbol->volatile_register)) {
        // if the value has already been loaded into a volatile that hasn't gone stale then we can just forgo this
        return -1;
    }

    if (info.spilled) {
        // is the variable stored on the stack then we want to load it from the stack like normal
        auto volatile_reg = vm->volatile_register();
        add_instruction(OP_LOAD, volatile_reg, info.spill_offset, {0, 0, 0, 0});
        symbol->volatile_register = volatile_reg;

    }else {
        // is the variable stored in a register then we want to do nothing and just return the register

        // NOTE: ANY FUNCTIONS IN THE VM THAT HAS TO DO WITH VOLATILES NEED TO HAVE ERROR CHECKING FOR IF A NON VOLATILE IS PASSED IN
        // BECAUSE OF THIS
        symbol->volatile_register = info.register_assignment;
    }

    return -1;
}

ui32 SageCodeGenVisitor::build_constant_int(int value) {
    auto success = symbol_table.declare_symbol(to_string(value), SageValue(64, value, SageBuiltinType(INT)));
    if (!success) {
        return symbol_table.lookup_id(to_string(value));
    }

    int result_reg = vm->volatile_register();
    add_instruction(OP_MOV, value, result_reg, {0, 0, 0, 0});

    symbol_table[symbol_table.size()-1]->volatile_register = result_reg;
    symbol_table[symbol_table.size()-1]->is_variable = false;

    // if value was successfully added to the symbol table then it was pushed back to the symbol table in the last spot
    return static_cast<ui32>(symbol_table.size()-1);
}

ui32 SageCodeGenVisitor::build_constant_float(float value) {
    SageType valuetype = SageBuiltinType(FLOAT);
    auto success = symbol_table.declare_symbol(to_string(value), SageValue(64, value, valuetype));
    if (!success) {
        return symbol_table.lookup_id(to_string(value));
    }

    int result_reg = vm->volatile_register();
    int heap_pointer = vm->store_in_heap(symbol_table.lookup(to_string(value))->value);
    add_instruction(OP_MOV, heap_pointer, result_reg, {0, 0, 0, 0});

    symbol_table[symbol_table.size()-1]->volatile_register = result_reg;
    symbol_table[symbol_table.size()-1]->is_variable = false;

    return static_cast<ui32>(symbol_table.size()-1);
}

ui32 SageCodeGenVisitor::build_string_pointer(string value) {
    SageType valuetype = SageArrayType(SageBuiltinType(CHAR), value.size());
    auto success = symbol_table.declare_symbol(value, SageValue(64, value, valuetype)));
    if (!success) {
        return symbol_table.lookup_id(value);
    }
    int heap_pointer = store_in_heap(symbol_table.lookup(value)->value);
    int result_reg = vm->volatile_register();
    add_instruction(OP_MOV, heap_pointer, result_reg, {0, 0, 0, 0});

    symbol_table[symbol_table.size()-1]->volatile_register = result_reg;
    symbol_table[symbol_table.size()-1]->is_variable = false;

    return static_cast<ui32>(symbol_table.size()-1);
}

ui32 SageCodeGenVisitor::build_function_call(
    vector<ui32> args, string function_name
) {
    // if there are more than 6 args then throw and unimplemented error
    if (args.size() > 6) {
        // ERROR: MORE THAN 6 FUNCTION PARAMETERS UNIMPLEMENTED 
        return -1;
    }
    // TODO: need to add check for recursive calls so we can make sure to generate bytecode for that case

    // move all args into the argument registers
    SageSymbol* symbol;
    VariableLifetime info;
    for (int i = 0; i < 6; ++i) {
        symbol = symbol_table.lookup_id(args[i]);
        // if is variable, if is parameter, if is volatile, if value is not null
        if (!vm->volatile_is_stale(symbol->volatile_register)) {
            add_instruction(OP_MOV, symbol->volatile_regster, i, {1, 0, 0, 0});

        }else if (symbol->is_parameter) {
            // FIX: TODO; we do not properly implement parameter register stack caching yet, so this kind of function call is broken right now
            // need to save previous function registers onto the stack before calling the next function

        }else if (symbol->is_variable) {
            // if the volatile register didn't go off
            // then that means that the variable hasn't been loaded from the stack
            info = analysis->variable_lifetimes[args[i]]
            add_instruction(OP_LOAD, i, info.spill_offset, {0, 0, 0, 0});
        }else if (!symbol->value.is_null()) {
            switch (symbol->value.valuetype) {
                case INT:
                    add_instruction(OP_MOV, symbol->value->value->int_value, i, {0, 0, 0, 0});
                    break;
                case FLOAT: {
                    int heap_pointer = store_in_heap(symbol->value);
                    add_instruction(OP_MOV, heap_pointer, i, {0, 0, 0, 0});
                    break;
                }
                case BOOL:
                    add_instruction(OP_MOV, symbol->value->value->bool_value, i, {0, 0, 0, 0});
                    break;
                case CHAR:
                    add_instruction(OP_MOV, symbol->value->value->char_value, i, {0, 0, 0, 0});
                    break;
                case POINTER:
                    add_instruction(OP_MOV, symbol->value->value->int_value, i, {0, 0, 0, 0});
                    break;
                case ARRAY:
                    // TODO:
                case FUNCTION:
                    // TODO:
                default:
                    // ERROR: prob a compiler bug
                    break;
            }

        }else {
            // ERROR: i think this would indicate a compiler bug??
            return -1;
        }
    }

    // create call instruction to bytecode procedure
    int procedure_encoding = vm->procedure_label_encoding[function_name]
    add_instruction(OP_CALL, procedure_encoding);

    return -1;
}

ui32 SageCodeGenVisitor::build_begin() {
    add_instruction(OP_BEGIN_EXECUTION, 0);
    last_begin_program_pos = procedures[current_procedure.top()].size()-1;
    return -1;
}

ui32 SageCodeGenVisitor::build_end() {
    add_instruction(OP_END_EXECUTION, 0);

    vm->load_program(procedures[current_procedure.top()]);
    vm->execute(last_begin_program_pos);

    return -1;
}



void SageCodeGenVisitor::add_instruction(SageOpCode opcode, int op1) {
    procedures[current_procedure.top()].push_back(command(opcode, op1, {0,0,0,0}));
}

void SageCodeGenVisitor::add_instruction(SageOpCode opcode, int op1, int map[4]) {
    procedures[current_procedure.top()].push_back(command(opcode, op1, map));
}

void SageCodeGenVisitor::add_instruction(SageOpCode opcode, int op1, int op2, int map[4]) {
    procedures[current_procedure.top()].push_back(command(opcode, op1, op2, map));
}

void SageCodeGenVisitor::add_instruction(SageOpCode opcode, int op1, int op2, int op3, int map[4]) {
    procedures[current_procedure.top()].push_back(command(opcode, op1, op2, op3, map));
}

void SageCodeGenVisitor::add_instruction(SageOpCode opcode, int op1, int op2, int op3, int op4, int map[4]) {
    procedures[current_procedure.top()].push_back(command(opcode, op1, op2, op3, op4, map));
}





















