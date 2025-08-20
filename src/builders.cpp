#include "../include/symbols.h"
#include "../include/interpreter.h"
#include "../include/node_manager.h"
#include "../include/codegen.h"
#include "../include/sage_bytecode.h"
#include "../include/bytecode_builder.h"

std::unordered_map<CanonicalType, std::unique_ptr<SageType>> TypeRegistery::builtin_types;
std::unordered_map<SageType*, std::unique_ptr<SageType>> TypeRegistery::pointer_types;
std::unordered_map<std::pair<SageType*, int>, std::unique_ptr<SageType>> TypeRegistery::array_types;
std::unordered_map<std::pair<std::vector<SageType*>, std::vector<SageType*>>, std::unique_ptr<SageType>> TypeRegistery::function_types;

BytecodeBuilder::BytecodeBuilder() {
    procedures[0] = procedure_frame("global");
    procedure_stack.push(0);
}

void BytecodeBuilder::new_frame(string name) {
    auto frame = procedure_frame(name);
    int id = procedures.size();
    procedures[id] = frame;
    procedure_stack.push(id);
}

void BytecodeBuilder::exit_frame() {
    procedure_stack.pop();
}

void BytecodeBuilder::reset() {
    procedures.clear();
    procedure_stack = stack<int>();

    procedures[0] = procedure_frame("global", bytecode());
    procedure_stack.push(0);
}

int BytecodeBuilder::current_id() {
    return procedure_stack.top();
}

bytecode BytecodeBuilder::final() {
    // TODO:
}

void BytecodeBuilder::add_instruction(SageOpCode opcode, int op1) {
    procedures[current_id()].procedure_instructions.push_back(command(opcode, op1, blank_encoding));
}

void BytecodeBuilder::add_instruction(SageOpCode opcode, int op1, int (&map)[4]) {
    procedures[current_id()].procedure_instructions.push_back(command(opcode, op1, map));
}

void BytecodeBuilder::add_instruction(SageOpCode opcode, int op1, int op2, int (&map)[4]) {
    procedures[current_id()].procedure_instructions.push_back(command(opcode, op1, op2, map));
}

void BytecodeBuilder::add_instruction(SageOpCode opcode, int op1, int op2, int op3, int (&map)[4]) {
    procedures[current_id()].procedure_instructions.push_back(command(opcode, op1, op2, op3, map));
}

void BytecodeBuilder::add_instruction(SageOpCode opcode, int op1, int op2, int op3, int op4, int (&map)[4]) {
    procedures[current_id()].procedure_instructions.push_back(command(opcode, op1, op2, op3, op4, map));
}



ui32 SageCompiler::build_store(ui32 rhs, string variable_symbol) {
    // find where variable_symbol is stored
    SageSymbol* expr_symbol = symbol_table.lookup(rhs);
    SageSymbol* var_symbol = symbol_table.lookup(variable_symbol);

    int stored_value = expr_symbol->assigned_register;
    if (expr_symbol->assigned_register == -1) {
        stored_value = 0;
        // ERROR!!: GOOD SPOT FOR LOGGING MONITORING, THIS SHOULDNT TRIGGER EVER
    }

    if (var_symbol->spilled) {
        builder.add_instruction(OP_STORE, stored_value, var_symbol->spill_offset, builder.first_encoding);
        return 0;
    }

    // otherwise the variable is stored in a register
    builder.add_instruction(OP_MOV, stored_value, var_symbol->assigned_register, builder.first_encoding)

    return 0;
}

ui32 SageCompiler::build_return(ui32 return_value_id) {
    // TODO: doesn't yet support multiple return values
    // this is why we are just by default loading the 
    // return value into sr6 because sr6 is the first 
    // return register.

    if (return_value == -1) {
        // there is no value to return
        builder.add_instruction(OP_RET, 0);
        builder.exit_frame();
        return 0;
    }

    // otherwise there is a return value
    SageSymbol* return_value = symbol_table.lookup(return_value_id);

    // literal expr
    //   in register
    // variable
    //   on stack
    //   in register
    // parameter
    //   in register

    if (return_value->is_variable && return_value->spilled) {
        // load the variable value into a volatile from the stack using the varlifetime offset
        builder.add_instruction(OP_LOAD, 6, return_value->spill_offset, builder.blank_encoding);
        builder.add_instruction(OP_RET, 0);

        builder.exit_frame();

        return 0;
    }

    // TODO: once we support more than 4 return values then we will need to add logic that checks if the parameter is spilled

    // move the value into the function return register
    builder.add_instruction(OP_MOV, return_value->assigned_register, 6, builder.first_encoding);
    builder.add_instruction(OP_RET, 0);

    builder.exit_frame();

    return 0;
}

ui32 SageCompiler::build_function_with_block(
    vector<string> argument_names, string function_name
) {
    // FIX: do we need to merge this with the bytecode builder?
    interpreter->procedure_label_encoding[function_name] = interpreter->procedure_encoding;
    interpreter->procedure_encoding++;

    ui32 symbol_id = symbol_table.lookup_id(function_name);
    // command procedure_label = command(OP_LABEL, symbol_id, encoding);

    builder.new_frame(function_name);
    builder.add_instruction(OP_LABEL, symbol_id, builder.blank_encoding)

    return 0;
}

ui32 SageCompiler::build_alloca(SageType* type, string var_name) {
    SageSymbol* variable = symbol_table.lookup(var_name);

    if (variable->spilled) {
        // then we can do normal stack stuff
        builder.add_instruction(OP_ADD, STACK_POINTER, STACK_POINTER, 1, builder.blank_encoding);
    }

    // otherwise its actually stored in a register and we dont' need to do anything
    return 0;
}

ui32 SageCompiler::build_operator(ui32 value1_id, ui32 value2_id, SageOpCode opcode) {
    SageSymbol* value1 = symbol_table.lookup(value1_id);
    SageSymbol* value2 = symbol_table.lookup(value2_id);
    int value1_register;
    int value2_register;

    if (value1->spilled) {
        value1_register = interpreter->get_volatile_register();
        builder.add_instruction(OP_LOAD, value1_register, value1->spill_offset, builder.blank_encoding);
    }

    if (value2->spilled) {
        value2_register = interpreter->get_volatile_register();
        builder.add_instruction(OP_LOAD, value2_register, value2->spill_offset, builder.blank_encoding);
    }

    int result_register = interpreter->get_volatile_register();
    builder.add_instruction(opcode, result_register, value1_register, value2_register, builder.blank_encoding);

    ui32 result_symbol_id = symbol_table.declare_internal_symbol(result_register);
    if (result_symbol_id == -1) {
        // ERROR??
        return 0;
    }

    return result_symbol_id;
}

ui32 SageCompiler::build_add(ui32 value1, ui32 value2) {
    return build_operator(value1, value2, OP_ADD);
}

ui32 SageCompiler::build_sub(ui32 value1, ui32 value2) {
    return build_operator(value1, value2, OP_SUB);
}

ui32 SageCompiler::build_mul(ui32 value1, ui32 value2) {
    return build_operator(value1, value2, OP_MUL);
}

ui32 SageCompiler::build_div(ui32 value1, ui32 value2) {
    return build_operator(value1, value2, OP_DIV);
}

ui32 SageCompiler::build_and(ui32 value1, ui32 value2) {
    return build_operator(value1, value2, OP_AND);
}

ui32 SageCompiler::build_or(ui32 value1, ui32 value2) {
    return build_operator(value1, value2, OP_OR);
}

ui32 SageCompiler::build_load(SageType* type, string reference_name) {
    SageSymbol* symbol = symbol_table.lookup(reference_name);

    if (symbol->spilled) {
        int volatile_reg = interpreter->get_volatile_register();
        builder.add_instruction(OP_LOAD, volatile_reg, symbol->spill_offset, builder.blank_encoding);
        symbol->assigned_regster = volatile_reg;
        return symbol_table.lookup_id(reference_name);
    }

    return symbol_table.lookup_id(reference_name);
}

ui32 SageCompiler::build_constant_int(int value) {
    auto* builtin_type = TypeRegistery::get_builtin_type(I64);
    auto success = symbol_table.declare_symbol(to_string(value), SageValue(64, value, builtin_type));
    if (!success) {
        return symbol_table.lookup_id(to_string(value));
    }

    int result_reg = interpreter->get_volatile_register();
    builder.add_instruction(OP_MOV, value, result_reg, builder.blank_encoding);

    symbol_table.symbol_table[symbol_table.size()-1]->assigned_register = result_reg;
    symbol_table.symbol_table[symbol_table.size()-1]->is_variable = false;
    symbol_table.symbol_table[symbol_table.size()-1]->is_parameter = false;

    // if value was successfully added to the symbol table then it was pushed back to the symbol table in the last spot
    // return static_cast<ui32>(symbol_table.size()-1);
    return symbol_table.lookup_id(to_string(value));
}

ui32 SageCompiler::build_constant_float(float value) {
    auto* builtin_type = TypeRegistery::get_builtin_type(F64);
    auto success = symbol_table.declare_symbol(to_string(value), SageValue(64, value, builtin_type));
    if (!success) {
        return symbol_table.lookup_id(to_string(value));
    }

    int result_reg = interpreter->get_volatile_register();
    int heap_pointer = interpreter->store_in_heap(symbol_table.lookup(to_string(value))->value); // FIX: should use stack instead, but its not majorly important for now
    builder.add_instruction(OP_MOV, heap_pointer, result_reg, builder.blank_encoding);

    symbol_table.symbol_table[symbol_table.size()-1]->assigned_register = result_reg;
    symbol_table.symbol_table[symbol_table.size()-1]->is_variable = false;
    symbol_table.symbol_table[symbol_table.size()-1]->is_parameter = false;

    return symbol_table.lookup_id(to_string(value));
}

ui32 SageCompiler::build_string_pointer(string value) {
    auto* char_type = TypeRegistery::get_builtin_type(CHAR);
    auto* array_type = TypeRegistery::get_pointer_type(char_type);
    string* strcopy = new string(value);
    auto success = symbol_table.declare_symbol(value, SageValue(64, strcopy, array_type));
    if (!success) {
        return symbol_table.lookup_id(value);
    }
    SageValue& symbol =  symbol_table.lookup(value)->value;
    int heap_pointer = interpreter->store_in_heap(symbol); // FIX: should use stack instead but its not majorly important for now
    int result_reg = interpreter->get_volatile_register();
    builder.add_instruction(OP_MOV, heap_pointer, result_reg, builder.blank_encoding);

    symbol_table.symbol_table[symbol_table.size()-1]->assigned_register = result_reg;
    symbol_table.symbol_table[symbol_table.size()-1]->is_variable = false;
    symbol_table.symbol_table[symbol_table.size()-1]->is_parameter = false;

    return symbol_table.lookup_id(value);
}

ui32 SageCompiler::build_function_call(vector<ui32> args, string function_name) {
    if (args.size() > 6) {
        // ERROR: MORE THAN 6 FUNCTION PARAMETERS UNIMPLEMENTED
        return 0;
    }

    SageSymbol* symbol;
    for (int i = 0; i < 6; ++i) {
        symbol = symbol_table.lookup(args[i]);

        if (symbol->spilled) {
            // is either a parameter or variable
            int vol_register = interpreter->get_volatile_register();
            builder.add_instruction(OP_LOAD, vol_register, symbol->spill_offset, builder.blank_encoding);
            builder.add_instruction(OP_MOV, vol_register, i, builder.blank_encoding);
            continue;
        }

        builder.add_instruction(OP_MOV, symbol->assigned_register, i, builder.blank_encoding);
    }

    int procedure_encoding = interpreter->procedure_label_encoding[function_name];
    builder.add_instruction(OP_CALL, procedure_encoding);
    return 0;
}
























