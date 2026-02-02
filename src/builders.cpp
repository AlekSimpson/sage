#include <atomic>
#include <cassert>

#include "../include/platform.h"
#include <cstring>

#include "../include/bytecode_builder.h"
#include "../include/symbols.h"
#include "../include/interpreter.h"
#include "../include/node_manager.h"
#include "../include/codegen.h"
#include "../include/sage_bytecode.h"

std::unordered_map<std::pair<CanonicalType, int>, std::unique_ptr<SageType> > TypeRegistery::builtin_types;
std::unordered_map<SageType *, std::unique_ptr<SageType> > TypeRegistery::pointer_types;
std::unordered_map<std::pair<SageType *, int>, std::unique_ptr<SageType> > TypeRegistery::array_types;
std::unordered_map<std::pair<std::vector<SageType *>, std::vector<SageType *> >, std::unique_ptr<SageType> >
TypeRegistery::function_types;
std::unordered_map<std::string, std::unique_ptr<SageType>> TypeRegistery::struct_types;

int get_procedure_frame_id(const std::string &str) {
    uint64_t hash = 5381;
    for (char c: str) {
        hash = ((hash << 5) + hash) + c;
    }
    return static_cast<int>(hash);
}

BytecodeBuilder::BytecodeBuilder() {
    int global_id = get_procedure_frame_id("GLOBAL");
    comptime_procedures[global_id] = ProcedureFrame("GLOBAL");
    comptime_procedure_stack.push(global_id);

    runtime_procedures[global_id] = ProcedureFrame("GLOBAL");
    runtime_procedure_stack.push(global_id);

    builtins = {"puts", "puti"};

    build_puts();
    build_puti();
}

string BytecodeBuilder::emit() {
    string result;

    for (const auto &[id, frame]: comptime_procedures) {
        if (builtins.find(frame.name) != builtins.end()) continue;

        for (auto instruction: frame.procedure_instructions) {
            result += instruction.print() + "\n";
        }
    }

    for (const auto &[id, frame]: runtime_procedures) {
        for (auto instruction: frame.procedure_instructions) {
            result += instruction.print() + "\n";
        }
    }

    return result;
}

void BytecodeBuilder::print_bytecode(bytecode &code) {
    int count = 0;
    map<int, string> label_names;
    printf("\n");
    for (const auto &[id, frame]: get_active_procedures()) {
        label_names[id] = frame.name;
    }

    if (emitting_comptime) {
        printf("------[  COMPTIME CODE  ]------\n");
    }else {
        printf("------[  RUNTIME CODE  ]------\n");
    }
    for (auto instruction: code) {
        printf("%d: %s\n", count, instruction.print(&label_names).c_str());
        count++;
    }
    printf("------------\n");
}

map<int, ProcedureFrame> &BytecodeBuilder::get_active_procedures() {
    if (emitting_comptime) return comptime_procedures;
    return runtime_procedures;
}

stack<int> &BytecodeBuilder::get_active_procedure_stack() {
    if (emitting_comptime) return comptime_procedure_stack;
    return runtime_procedure_stack;
}

int &BytecodeBuilder::get_total_instruction_count() {
    if (emitting_comptime) return comptime_total_instructions;
    return runtime_total_instructions;
}

void BytecodeBuilder::increment_total_instruction_count(int delta) {
    if (emitting_comptime) {
        comptime_total_instructions += delta;
        return;
    }
    runtime_total_instructions += delta;
}

void BytecodeBuilder::enter_comptime() {
    emitting_comptime = true;
}

void BytecodeBuilder::reset_and_exit_comptime() {
    reset();
    emitting_comptime = false;
}

void BytecodeBuilder::new_frame(string name) {
    auto frame = ProcedureFrame(name);
    if (!emitting_comptime && name == "main") {
        runtime_has_main_function = true;
    }
    int id = get_procedure_frame_id(name);
    if (emitting_comptime) frame.is_comptime = true;

    auto &active_procedure_stack = get_active_procedure_stack();
    auto &active_procedures = get_active_procedures();
    active_procedures[id] = frame;
    active_procedure_stack.push(id);
}

void BytecodeBuilder::exit_frame() {
    auto &active_procedure_stack = get_active_procedure_stack();
    active_procedure_stack.pop();
}

void BytecodeBuilder::reset() {
    comptime_total_instructions = 0;
    runtime_total_instructions = 0;

    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures.clear();
    procedure_stack = stack<int>();

    int global_id = get_procedure_frame_id("GLOBAL");
    procedures[global_id] = ProcedureFrame("GLOBAL");
    procedure_stack.push(global_id);

    build_puts();
    build_puti();
}

bytecode BytecodeBuilder::finalize_comptime_bytecode(map<int, int> &procedure_line_locations) {
    int total_instruction_count = get_total_instruction_count();
    bytecode result;
    result.reserve(total_instruction_count);

    for (const auto &[id, frame]: comptime_procedures) {
        string procedure_name = frame.name;
        if (procedure_name == "GLOBAL") continue;

        bytecode current_instructions = frame.procedure_instructions;
        procedure_line_locations[id] = result.size();

        result.insert(result.end(), current_instructions.begin(), current_instructions.end());
    }

    int global_id = get_procedure_frame_id("GLOBAL");
    auto global_instructions = comptime_procedures[global_id].procedure_instructions;
    procedure_line_locations[global_id] = result.size();
    result.insert(result.end(), global_instructions.begin(), global_instructions.end());
    result.push_back(Command(VOP_EXIT, 0, _00));
    return result;
}

bytecode BytecodeBuilder::finalize_runtime_bytecode(map<int, int> &procedure_line_locations) {
    int total_instruction_count = get_total_instruction_count();
    int neutral_operand_encoding[4] = {0, 0, 0, 0};
    bytecode result;
    result.reserve(total_instruction_count);

    for (const auto &[id, frame]: runtime_procedures) {
        string procedure_name = frame.name;
        if (procedure_name == "GLOBAL") continue;

        bytecode current_instructions = frame.procedure_instructions;
        procedure_line_locations[id] = result.size();

        result.insert(result.end(), current_instructions.begin(), current_instructions.end());
    }

    int global_id = get_procedure_frame_id("GLOBAL");
    auto global_instructions = runtime_procedures[global_id].procedure_instructions;

    if (runtime_has_main_function) {
        // interpreter program pointer always starts at procedure_line_locations[id("GLOBAL")]
        // but we want runtime execution to start at "main" function so we point the global starter to the main func line location
        procedure_line_locations[global_id] = procedure_line_locations[get_procedure_frame_id("main")];
    }

    result.insert(result.end(), global_instructions.begin(), global_instructions.end());
    result.push_back(Command(VOP_EXIT, -1, neutral_operand_encoding));
    return result;
}

void BytecodeBuilder::build_instruction(SageOpCode opcode, int operand1, int operand2, int operand3, AddressMode mode) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        opcode,
        operand1,
        operand2,
        operand3,
        mode
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_instruction(SageOpCode opcode, int operand1, int operand2, AddressMode mode) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        opcode,
        operand1,
        operand2,
        mode
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_instruction(SageOpCode opcode, int operand, AddressMode mode) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        opcode,
        operand,
        mode
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_builtin_instruction(SageOpCode opcode, int operand1, int operand2, int operand3, AddressMode mode) {
    runtime_procedures[runtime_procedure_stack.top()].procedure_instructions.push_back(Command(
        opcode,
        operand1,
        operand2,
        operand3,
        mode
    ));
    runtime_total_instructions++;

    comptime_procedures[comptime_procedure_stack.top()].procedure_instructions.push_back(Command(
        opcode,
        operand1,
        operand2,
        operand3,
        mode
    ));
    comptime_total_instructions++;
}

void BytecodeBuilder::build_builtin_instruction(SageOpCode opcode, int operand1, int operand2, AddressMode mode) {
    runtime_procedures[runtime_procedure_stack.top()].procedure_instructions.push_back(Command(
        opcode,
        operand1,
        operand2,
        mode
    ));
    runtime_total_instructions++;

    comptime_procedures[comptime_procedure_stack.top()].procedure_instructions.push_back(Command(
        opcode,
        operand1,
        operand2,
        mode
    ));
    comptime_total_instructions++;
}

void BytecodeBuilder::build_builtin_instruction(SageOpCode opcode, int operand, AddressMode mode) {
    runtime_procedures[runtime_procedure_stack.top()].procedure_instructions.push_back(Command(
        opcode,
        operand,
        mode
    ));
    runtime_total_instructions++;

    comptime_procedures[comptime_procedure_stack.top()].procedure_instructions.push_back(Command(
        opcode,
        operand,
        mode
    ));
    comptime_total_instructions++;
}

void BytecodeBuilder::build_fload(int float_register, int offset) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        OP_FLOAD,
        float_register,
        offset,
        _00
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_load(int sage_register, int offset) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        OP_LOAD,
        sage_register,
        offset,
        _00
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_store_immediate(int offset, int immediate) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        OP_STORE,
        offset,
        immediate,
        _00
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_store_register(int offset, int sage_register) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        OP_STORE,
        offset,
        sage_register,
        _01
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_fstore_register(int offset, int float_register) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        OP_FSTORE,
        offset,
        float_register,
        _01
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_fmove_register(int destination_register, int source_register) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        OP_FMOV,
        destination_register,
        source_register,
        _01
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_move_immediate(int sage_register, int immediate) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        OP_MOV,
        sage_register,
        immediate,
        _00
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_move_register(int destination_register, int source_register) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        OP_MOV,
        destination_register,
        source_register,
        _01
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_not(int sage_register) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        OP_NOT,
        sage_register,
        _01
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_puti() {
    // puts(r0=integer, r1=digit_count)

    int id = get_procedure_frame_id("puti");
    runtime_procedures[id] = ProcedureFrame("puti");
    comptime_procedures[id] = ProcedureFrame("puti");
    runtime_procedure_stack.push(id);
    comptime_procedure_stack.push(id);
    build_builtin_instruction(OP_LABEL, id, _00);
    build_builtin_instruction(OP_MOV, 22, SYS_WRITE_INT, _00);
    build_builtin_instruction(OP_MOV, 10, 1, _01);  // save digit count (r1) in temp register r10
    build_builtin_instruction(OP_MOV, 1, 0, _01); // save integer to print into r1
    build_builtin_instruction(OP_MOV, 2, 10, _01); // save digit count into r2
    build_builtin_instruction(OP_MOV, 0, STDOUT_FILENO, _00); // tell system that this is outputting to stdout
    build_builtin_instruction(OP_SYSCALL, -1, _00);
    build_builtin_instruction(OP_RET, -1, _00);
    runtime_procedure_stack.pop();
    comptime_procedure_stack.pop();
}

void BytecodeBuilder::build_puts() {
    // puts(r0=characters, r1=char_count)

    int id = get_procedure_frame_id("puts");
    runtime_procedures[id] = ProcedureFrame("puts");
    comptime_procedures[id] = ProcedureFrame("puts");
    runtime_procedure_stack.push(id);
    comptime_procedure_stack.push(id);
    build_builtin_instruction(OP_LABEL, id, _00);
    build_builtin_instruction(OP_MOV, 22, SYS_WRITE, _00);
    build_builtin_instruction(OP_MOV, 10, 1, _01);  // save digit count (r1) in temp register r10
    build_builtin_instruction(OP_MOV, 1, 0, _01);  // save character buff pointer to print into r1
    build_builtin_instruction(OP_MOV, 2, 10, _01);  // save digit count into r2
    build_builtin_instruction(OP_MOV, 0, STDOUT_FILENO, _00);  // tell system that this is outputting to stdout
    build_builtin_instruction(OP_SYSCALL, -1, _00);
    build_builtin_instruction(OP_RET, -1, _00);
    runtime_procedure_stack.pop();
    comptime_procedure_stack.pop();
}

VisitorResult SageCompiler::build_store(VisitorResult right_value, symbol_entry *var_symbol) {
    auto right_value_state = right_value.get_result_state(&symbol_table);
    auto variable_symbol_state = var_symbol->spilled ? VisitorResultState::SPILLED : VisitorResultState::REGISTER ;

    int control_flow_path = ((int)right_value_state * 4) + (int)variable_symbol_state;
    switch (control_flow_path) {
        case 1: {
            /* right_value=IMMEDIATE, var_symbol=SPILLED */
            builder.build_store_immediate(var_symbol->spill_offset, right_value.immediate_value);

            break;
        }
        case 5: {
            /* right_value=SPILLED, var_symbol=SPILLED */
            symbol_entry *right_variable_symbol = symbol_table.lookup_by_index(right_value.symbol_table_index);

            if (var_symbol->type->identify() == FLOAT || right_variable_symbol->type->identify() == FLOAT) {
                int float_register = get_volatile_float_register();
                builder.build_fload(float_register, right_variable_symbol->spill_offset);
                builder.build_fstore_register(var_symbol->spill_offset, float_register);
            }else {
                int temporary_register = get_volatile_register();
                builder.build_load(temporary_register, right_variable_symbol->spill_offset);
                builder.build_store_register(var_symbol->spill_offset, temporary_register);
            }

            break;
        }
        case 9: {
            /* right_value=REGISTER, var_symbol=SPILLED */
            symbol_entry *right_variable_symbol = symbol_table.lookup_by_index(right_value.symbol_table_index);

            if (var_symbol->type->identify() == FLOAT || right_variable_symbol->type->identify() == FLOAT) {
                builder.build_fstore_register(var_symbol->spill_offset, right_variable_symbol->assigned_register);
            }else {
                builder.build_store_register(var_symbol->spill_offset, right_variable_symbol->assigned_register);
            }

            break;
        }
        case 13: {
            /* right_value=VALUE, var_symbol=SPILLED */
            symbol_entry *right_variable_symbol = symbol_table.lookup_by_index(right_value.symbol_table_index);

            if (var_symbol->type->identify() == FLOAT) {
                int static_pointer = get_literal_static_pointer(right_variable_symbol->symbol_id);
                int float_register = get_volatile_float_register();
                builder.build_fload(float_register, static_pointer);
                builder.build_fstore_register(var_symbol->spill_offset, float_register);
            }else {
                builder.build_store_immediate(var_symbol->spill_offset, right_variable_symbol->value);
            }

            break;
        }
        case 2: {
            /* right_value=IMMEDIATE, var_symbol=REGISTER */
            if (var_symbol->type->identify() == FLOAT) {
                int float_register = get_volatile_float_register();
                builder.build_move_immediate(float_register, right_value.immediate_value);
                builder.build_fmove_register(var_symbol->assigned_register, right_value.immediate_value);
            }else {
                builder.build_move_immediate(var_symbol->assigned_register, right_value.immediate_value);
            }
            break;
        }
        case 6: {
            /* right_value=SPILLED, var_symbol=REGISTER */
            SageOpCode opcode = var_symbol->type->identify() == FLOAT ? OP_FLOAD : OP_LOAD;
            symbol_entry *right_variable_symbol = symbol_table.lookup_by_index(right_value.symbol_table_index);
            builder.build_instruction(opcode, var_symbol->assigned_register, right_variable_symbol->spill_offset, _00);

            break;
        }
        case 10: {
            /* right_value=REGISTER, var_symbol=REGISTER */
            symbol_entry *right_variable_symbol = symbol_table.lookup_by_index(right_value.symbol_table_index);
            if (var_symbol->type->identify() == FLOAT) {
                builder.build_fmove_register(var_symbol->assigned_register, right_variable_symbol->assigned_register);
            }else {
                builder.build_move_register(var_symbol->assigned_register, right_variable_symbol->assigned_register);
            }
            break;
        }
        case 14: {
            /* right_value=VALUE, var_symbol=REGISTER */
            symbol_entry *right_variable_symbol = symbol_table.lookup_by_index(right_value.symbol_table_index);
            if (var_symbol->type->identify() == FLOAT) {
                int static_pointer = get_literal_static_pointer(right_variable_symbol->symbol_id);
                int float_register = get_volatile_float_register();
                builder.build_fload(float_register, static_pointer);
                builder.build_fstore_register(var_symbol->spill_offset, float_register);
            }else {
                builder.build_store_immediate(var_symbol->spill_offset, right_variable_symbol->value);
                builder.build_move_immediate(var_symbol->assigned_register, right_variable_symbol->value);
            }
            break;
        }
        default:
            break;
    }
    return VisitorResult();
}

VisitorResult SageCompiler::build_return(VisitorResult return_value, bool is_program_exit) {
    // TODO: doesn't yet support multiple return values
    // TODO: float return values not supported yet

    SageOpCode opcode = OP_RET;
    if (is_program_exit) {
        opcode = VOP_EXIT;
    }

    if (return_value.is_null()) {
        builder.build_instruction(opcode, 0, _00);
        builder.exit_frame();
        return VisitorResult();
    }

    constexpr int RETURN_REGISTER = 6;
    if (return_value.is_immediate()) {
        assertm(!TypeRegistery::is_float64_type(return_value.immediate_value.valuetype), "Float return types are not supported yet.");
        builder.build_instruction(OP_MOV, RETURN_REGISTER, return_value.immediate_value, _00);
        builder.build_instruction(opcode, 0, _00);
        builder.exit_frame();
        return VisitorResult();
    }
    auto *return_symbol = symbol_table.lookup_by_index(return_value.symbol_table_index);
    assertm(!TypeRegistery::is_float64_type(return_symbol->type), "Float return types are not supported yet.");

    switch (return_value.get_result_state(&symbol_table)) {
        case VisitorResultState::IMMEDIATE: {
            builder.build_move_immediate(RETURN_REGISTER, return_value.immediate_value);
            break;
        }
        case VisitorResultState::SPILLED: {
            int temporary_register = get_volatile_register();
            builder.build_load(temporary_register, return_symbol->spill_offset);
            builder.build_instruction(OP_MOV, RETURN_REGISTER, temporary_register, _01);
            break;
        }
        case VisitorResultState::REGISTER: {
            builder.build_instruction(OP_MOV, RETURN_REGISTER, return_symbol->assigned_register, _01);
            break;
        }
        case VisitorResultState::VALUE: {
            builder.build_instruction(OP_MOV, RETURN_REGISTER, return_symbol->value, _00);
            break;
        }
        default:
            break;
    }

    builder.build_instruction(opcode, 0, _00);
    builder.exit_frame();
    return VisitorResult();
}

VisitorResult SageCompiler::build_function_with_block(string function_name) {
    builder.new_frame(function_name);
    builder.build_instruction(OP_LABEL, get_procedure_frame_id(function_name), _00);
    return VisitorResult();
}

VisitorResult SageCompiler::build_alloca(symbol_entry *var_symbol) {
    if (var_symbol->spilled) {
        int offset = var_symbol->type->alignment;
        builder.build_instruction(OP_SUB, STACK_POINTER, STACK_POINTER, offset, _10);
    }

    return VisitorResult();
}

bool SageCompiler::is_float_operation(VisitorResult &one, VisitorResult &two) {
    if (one.is_immediate() && one.immediate_value.valuetype->identify() == FLOAT) return true;
    if (two.is_immediate() && two.immediate_value.valuetype->identify() == FLOAT) return true;

    if (one.symbol_table_index != SAGE_NULL_SYMBOL) {
        auto *entry = symbol_table.lookup_by_index(one.symbol_table_index);
        if (entry && entry->type->identify() == FLOAT) return true;
    }

    if (two.symbol_table_index != SAGE_NULL_SYMBOL) {
        auto *entry = symbol_table.lookup_by_index(two.symbol_table_index);
        if (entry && entry->type->identify() == FLOAT) return true;
    }

    return false;
}

int SageCompiler::materialize_to_float_register(VisitorResult &value) {
    auto state = value.get_result_state(&symbol_table);
    int float_reg = get_volatile_float_register();

    switch (state) {
        case VisitorResultState::IMMEDIATE: {
            // Convert int immediate to float if needed
            float float_val;
            if (value.immediate_value.valuetype->identify() == FLOAT) {
                float_val = value.immediate_value.as_float();
            } else {
                float_val = static_cast<float>(value.immediate_value.as_i32());
            }

            // Reinterpret float bits as int32 so we can use integer store
            int32_t float_bits;
            memcpy(&float_bits, &float_val, sizeof(float));

            // Allocate stack space and store the float bits
            int offset = sizeof(float);
            builder.build_instruction(OP_SUB, STACK_POINTER, STACK_POINTER, offset, _10);
            builder.build_store_immediate(offset, float_bits);
            builder.build_fload(float_reg, offset);
            break;
        }
        case VisitorResultState::SPILLED: {
            symbol_entry *entry = symbol_table.lookup_by_index(value.symbol_table_index);
            if (entry->type->identify() == FLOAT) {
                builder.build_fload(float_reg, entry->spill_offset);
            } else {
                // Int is spilled - load to int register, then need int-to-float conversion
                int int_reg = get_volatile_register();
                builder.build_load(int_reg, entry->spill_offset);
                // TODO: emit int-to-float conversion instruction here
                logger.log_internal_error_unsafe(
                    "builders.cpp",
                    current_linenum,
                    "UNIMPLEMENTED: Float operation requires int-to-float cast instruction.");
                // builder.build_instruction(OP_ITOF, float_reg, int_reg, _01);
            }
            break;
        }
        case VisitorResultState::REGISTER: {
            symbol_entry *entry = symbol_table.lookup_by_index(value.symbol_table_index);
            if (entry->type->identify() == FLOAT) {
                builder.build_fmove_register(float_reg, entry->assigned_register);
            } else {
                // Int in register - need int-to-float conversion
                // TODO: emit int-to-float conversion instruction here
                logger.log_internal_error_unsafe(
                    "builders.cpp",
                    current_linenum,
                    "UNIMPLEMENTED: Float operation requires int-to-float cast instruction.");
                // builder.build_instruction(OP_ITOF, float_reg, entry->assigned_register, _01);
            }
            break;
        }
        case VisitorResultState::VALUE: {
            symbol_entry *entry = symbol_table.lookup_by_index(value.symbol_table_index);
            if (entry->type->identify() == FLOAT) {
                builder.build_fload(float_reg, entry->spill_offset);
            } else {
                // Compile-time int value - convert to float, spill to stack, load
                float float_val = static_cast<float>(entry->value.as_i32());

                int32_t float_bits;
                memcpy(&float_bits, &float_val, sizeof(float));

                int offset = sizeof(float);
                builder.build_instruction(OP_SUB, STACK_POINTER, STACK_POINTER, offset, _10);
                builder.build_store_immediate(offset, float_bits);
                builder.build_fload(float_reg, offset);
            }
            break;
        }
    }
    return float_reg;
}

VisitorResult SageCompiler::build_operator(
    VisitorResult value1, VisitorResult value2, SageOpCode opcode, bool is_float_operation) {

    if (is_float_operation) {
        int result_register = get_volatile_float_register();
        int register1 = materialize_to_float_register(value1);
        int register2 = materialize_to_float_register(value2);
        builder.build_instruction(opcode, result_register, register1, register2, _11);
    }
    int result_register = get_volatile_register();

    int control_path_id = ((int)value1.get_result_state(&symbol_table) * 4) + (int)value2.get_result_state(&symbol_table);
    switch (control_path_id) {
        case 0:
            /* value1=IMMEDIATE , value2=IMMEDIATE */
            builder.build_instruction(opcode, result_register, value1.immediate_value, value2.immediate_value, _00);
            break;
        case 1: {
            /* value1=IMMEDIATE , value2=SPILLED */
            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);
            int temporary_result = get_volatile_register();
            builder.build_load(temporary_result, value2_entry->spill_offset);
            builder.build_instruction(opcode, result_register, value1.immediate_value, temporary_result, _01);
            break;
        }
        case 2: {
            /* value1=IMMEDIATE , value2=REGISTER */
            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);
            builder.build_instruction(opcode, result_register, value1.immediate_value, value2_entry->assigned_register, _01);
            break;
        }
        case 3: {
            /* value1=IMMEDIATE , value2=VALUE */
            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);
            builder.build_instruction(opcode, result_register, value1.immediate_value, value2_entry->value, _00);
            break;
        }
        case 4: {
            /* value1=SPILLED, value2=IMMEDIATE */
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);
            int temporary_result = get_volatile_register();
            builder.build_load(temporary_result, value1_entry->spill_offset);
            builder.build_instruction(opcode, result_register, temporary_result, value2.immediate_value, _10);
            break;
        }
        case 5: {
            /* value1=SPILLED, value2=SPILLED */
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);
            int temporary_result1 = get_volatile_register();
            builder.build_load(temporary_result1, value1_entry->spill_offset);

            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);
            int temporary_result2 = get_volatile_register();
            builder.build_load(temporary_result2, value2_entry->spill_offset);

            builder.build_instruction(opcode, result_register, temporary_result1, temporary_result2, _11);
            break;
        }
        case 6: {
            /* value1=SPILLED, value2=REGISTER */
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);
            int temporary_result1 = get_volatile_register();
            builder.build_load(temporary_result1, value1_entry->spill_offset);

            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);
            builder.build_instruction(opcode, result_register, temporary_result1, value2_entry->assigned_register, _11);
            break;
        }
        case 7: {
            /* value1=SPILLED, value2=VALUE */
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);
            int temporary_result1 = get_volatile_register();
            builder.build_load(temporary_result1, value1_entry->spill_offset);

            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);
            builder.build_instruction(opcode, result_register, temporary_result1, value2_entry->value, _10);
            break;
        }
        case 8: {
            /* value1=REGISTER, value2=IMMEDIATE */
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);

            builder.build_instruction(opcode, result_register, value1_entry->assigned_register, value2.immediate_value, _10);
            break;
        }
        case 9: {
            /* value1=REGISTER, value2=SPILLED */
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);

            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);
            int temporary_result2 = get_volatile_register();
            builder.build_load(temporary_result2, value2_entry->spill_offset);

            builder.build_instruction(opcode, result_register, value1_entry->assigned_register, temporary_result2, _11);
            break;
        }
        case 10: {
            /* value1=REGISTER, value2=REGISTER */
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);
            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);

            builder.build_instruction(opcode, result_register, value1_entry->assigned_register, value2_entry->assigned_register, _11);
            break;
        }
        case 11: {
            /* value1=REGISTER, value2=VALUE */
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);
            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);

            builder.build_instruction(opcode, result_register, value1_entry->assigned_register, value2_entry->value, _10);
            break;
        }
        case 12: {
            /* value1=VALUE, value2=IMMEDIATE */
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);

            builder.build_instruction(opcode, result_register, value1_entry->value, value2.immediate_value, _00);
            break;
        }
        case 13: {
            /* value1=VALUE, value2=SPILLED */
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);
            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);
            int temporary_result2 = get_volatile_register();
            builder.build_load(temporary_result2, value2_entry->spill_offset);

            builder.build_instruction(opcode, result_register, value1_entry->value, temporary_result2, _01);
            break;
        }
        case 14: {
            /* value1=VALUE, value2=REGISTER */
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);
            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);

            builder.build_instruction(opcode, result_register, value1_entry->value, value2_entry->assigned_register, _01);
            break;
        }
        case 15: {
            /* value1=VALUE, value2=VALUE */
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);
            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);

            builder.build_instruction(opcode, result_register, value1_entry->value, value2_entry->value, _00);
            break;
        }
        default:
            logger.log_internal_error_unsafe(
                "builders.cpp",
                current_linenum,
                "build operator tried to execute an invalid operator control path.");
            break;
    }

    return VisitorResult((table_index)symbol_table.declare_temporary(result_register));
}

VisitorResult SageCompiler::build_add(VisitorResult value1, VisitorResult value2) {
    bool is_float = is_float_operation(value1, value2);
    return build_operator(value1, value2,  is_float ? OP_FADD : OP_ADD, is_float);
}

VisitorResult SageCompiler::build_sub(VisitorResult value1, VisitorResult value2) {
    bool is_float = is_float_operation(value1, value2);
    return build_operator(value1, value2, is_float ? OP_FSUB : OP_SUB, is_float);
}

VisitorResult SageCompiler::build_mul(VisitorResult value1, VisitorResult value2) {
    bool is_float = is_float_operation(value1, value2);
    return build_operator(value1, value2, is_float ? OP_FMUL : OP_MUL, is_float);
}

VisitorResult SageCompiler::build_div(VisitorResult value1, VisitorResult value2) {
    bool is_float = is_float_operation(value1, value2);
    return build_operator(value1, value2, is_float ? OP_FDIV : OP_DIV, is_float);
}

VisitorResult SageCompiler::build_and(VisitorResult value1, VisitorResult value2) {
    return build_operator(value1, value2, OP_AND, false);
}

VisitorResult SageCompiler::build_or(VisitorResult value1, VisitorResult value2) {
    return build_operator(value1, value2, OP_OR, false);
}

VisitorResult SageCompiler::build_load(NodeIndex reference_node) {
    string reference_name = node_manager->get_identifier(reference_node);
    int scope_id = node_manager->get_scope_id(reference_node);
    auto symbol = symbol_table.lookup(reference_name, scope_id);

    if (symbol->spilled) {
        int volatile_reg;
        if (symbol->type->identify() == FLOAT) {
            volatile_reg = get_volatile_float_register();
            builder.build_fload(volatile_reg, symbol->spill_offset);
            symbol->assigned_register = volatile_reg;
        }else {
            volatile_reg = get_volatile_register();
            builder.build_load(volatile_reg, symbol->spill_offset);
            symbol->assigned_register = volatile_reg;
        }
    }

    return VisitorResult((table_index)symbol_table.lookup_table_index(reference_name, scope_id));
}

void SageCompiler::process_escape_sequences(string &str) {
    size_t write_pos = 0;

    for (size_t read_pos = 0; read_pos < str.length(); ++read_pos) {
        if (str[read_pos] == '\\' && read_pos + 1 < str.length()) {
            switch (str[read_pos + 1]) {
                case 'n':
                    str[write_pos++] = '\n';
                    break;
                case 't':
                    str[write_pos++] = '\t';
                    break;
                case 'r':
                    str[write_pos++] = '\r';
                    break;
                case '\\':
                    str[write_pos++] = '\\';
                    break;
                case '"':
                    str[write_pos++] = '"';
                    break;
                case '0':
                    str[write_pos++] = '\0';
                    break;
                default:
                    str[write_pos++] = str[read_pos];
                    continue;
            }
            ++read_pos;
            continue;
        }

        str[write_pos++] = str[read_pos];
    }

    str.resize(write_pos);
}
