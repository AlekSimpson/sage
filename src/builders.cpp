#include <atomic>
#include <unistd.h>
#include <sys/syscall.h>

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

    build_puts();
    build_puti();
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
    result.push_back(command(VOP_EXIT, 0, _00));
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
    result.push_back(command(VOP_EXIT, -1, neutral_operand_encoding));
    return result;
}

void BytecodeBuilder::build_instruction(SageOpCode opcode, int operand1, int operand2, int operand3, AddressMode mode) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
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

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
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

    procedures[procedure_stack.top()].procedure_instructions.push_back(command(
        opcode,
        operand,
        mode
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_puti() {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    int id = get_procedure_frame_id("puti");
    procedures[id] = ProcedureFrame("puti");
    procedure_stack.push(id);
    build_instruction(OP_LABEL, id, _00);
    build_instruction(OP_MOV, 22, SAGESYS_write_int, _00);
    build_instruction(OP_MOV, 10, 1, _01);  // save digit count (r1) in temp register r10
    build_instruction(OP_MOV, 1, 0, _01); // save integer to print into r1
    build_instruction(OP_MOV, 2, 10, _01); // save digit count into r2
    build_instruction(OP_MOV, 0, STDOUT_FILENO, _00); // tell system that this is outputting to stdout
    build_instruction(OP_SYSCALL, -1, _00);
    build_instruction(OP_RET, -1, _00);
    procedure_stack.pop();
}

void BytecodeBuilder::build_puts() {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    int id = get_procedure_frame_id("puts");
    procedures[id] = ProcedureFrame("puts");
    procedure_stack.push(id);
    build_instruction(OP_LABEL, id, _00);
    build_instruction(OP_MOV, 22, SYS_write, _00);
    build_instruction(OP_MOV, 10, 1, _01);  // save digit count (r1) in temp register r10
    build_instruction(OP_MOV, 1, 0, _01);  // save integer to print into r1
    build_instruction(OP_MOV, 2, 10, _01);  // save digit count into r2
    build_instruction(OP_MOV, 0, STDOUT_FILENO, _00);  // tell system that this is outputting to stdout
    build_instruction(OP_SYSCALL, -1, _00);
    build_instruction(OP_RET, -1, _00);
    procedure_stack.pop();
}

VisitorResult SageCompiler::build_store(VisitorResult right_value, symbol_entry *var_symbol) {
    string for_debugging = var_symbol->identifier;
    auto right_value_state = right_value.get_result_state(&symbol_table);
    auto variable_symbol_state = var_symbol->spilled ? VisitorResultState::SPILLED : VisitorResultState::REGISTER ;

    int control_flow_path = ((int)right_value_state * 4) + (int)variable_symbol_state;
    switch (control_flow_path) {
        case 4: {
            /* right_value=IMMEDIATE, var_symbol=SPILLED */
            builder.build_instruction(OP_STORE, var_symbol->spill_offset, right_value.immediate_value, _00);
            break;
        }
        case 5: {
            /* right_value=SPILLED, var_symbol=SPILLED */
            symbol_entry *right_variable_symbol = symbol_table.lookup_by_index(right_value.symbol_table_index);
            int temporary_register = get_volatile();
            builder.build_instruction(OP_LOAD, temporary_register, right_variable_symbol->spill_offset);
            builder.build_instruction(OP_STORE, temporary_register, var_symbol->spill_offset);
            break;
        }
        case 6: {
            /* right_value=REGISTER, var_symbol=SPILLED */
            symbol_entry *right_variable_symbol = symbol_table.lookup_by_index(right_value.symbol_table_index);
            builder.build_instruction(OP_STORE, right_variable_symbol->assigned_register, var_symbol->spill_offset);
            break;
        }
        case 7: {
            /* right_value=VALUE, var_symbol=SPILLED */
            symbol_entry *right_variable_symbol = symbol_table.lookup_by_index(right_value.symbol_table_index);
            builder.build_instruction(OP_STORE, right_variable_symbol->value, var_symbol->spill_offset);
            break;
        }
        case 8: {
            /* right_value=IMMEDIATE, var_symbol=REGISTER */
            symbol_entry *right_variable_symbol = symbol_table.lookup_by_index(right_value.symbol_table_index);
            builder.build_instruction(OP_MOV, right_variable_symbol->assigned_register, var_symbol->assigned_register);
            break;
        }
        case 9: {
            /* right_value=SPILLED, var_symbol=REGISTER */
            symbol_entry *right_variable_symbol = symbol_table.lookup_by_index(right_value.symbol_table_index);
            int temporary_register = get_volatile();
            builder.build_instruction(OP_LOAD, temporary_register, right_variable_symbol->spill_offset);
            builder.build_instruction(OP_MOV, temporary_register, var_symbol->assigned_register);
            break;
        }
        case 10: {
            /* right_value=REGISTER, var_symbol=REGISTER */
            symbol_entry *right_variable_symbol = symbol_table.lookup_by_index(right_value.symbol_table_index);
            builder.build_instruction(OP_MOV, right_variable_symbol->assigned_register, var_symbol->assigned_register);
            break;
        }
        case 11: {
            /* right_value=VALUE, var_symbol=REGISTER */
            symbol_entry *right_variable_symbol = symbol_table.lookup_by_index(right_value.symbol_table_index);
            builder.build_instruction(OP_MOV, right_variable_symbol->value, var_symbol->assigned_register);
            break;
        }
        default:
            break;
    }
    return VisitorResult();
}

VisitorResult SageCompiler::build_return(VisitorResult return_value, bool is_program_exit) {
    // TODO: doesn't yet support multiple return values
    // this is why we are just by default loading the
    // return value into sr6 because sr6 is the first
    // return register.

    SageOpCode retcode = OP_RET;
    if (is_program_exit) {
        retcode = VOP_EXIT;
    }

    if (return_value.is_null()) {
        builder.build_instruction(retcode, 0);
        builder.exit_frame();
        return VisitorResult();
    }
    if (return_value.is_immediate()) {
        builder.build_instruction(OP_MOV, return_value.immediate_value, 6);
        builder.build_instruction(retcode, 0);
        builder.exit_frame();
        return VisitorResult();
    }

    switch (return_value.get_result_state(&symbol_table)) {
        case VisitorResultState::IMMEDIATE: {
            builder.build_instruction(OP_MOV, return_value.immediate_value, 6);
            break;
        }
        case VisitorResultState::SPILLED: {
            auto *return_symbol = symbol_table.lookup_by_index(return_value.symbol_table_index);
            int temporary_register = get_volatile();
            builder.build_instruction(OP_LOAD, temporary_register, return_symbol->spill_offset);
            builder.build_instruction(OP_MOV, temporary_register, 6);
            break;
        }
        case VisitorResultState::REGISTER: {
            auto *return_symbol = symbol_table.lookup_by_index(return_value.symbol_table_index);
            builder.build_instruction(OP_MOV, return_symbol->assigned_register, 6);
            break;
        }
        case VisitorResultState::VALUE: {
            auto *return_symbol = symbol_table.lookup_by_index(return_value.symbol_table_index);
            builder.build_instruction(OP_MOV, return_symbol->value, 6);
            break;
        }
        default:
            break;
    }

    builder.build_instruction(retcode, 0);
    builder.exit_frame();
    return VisitorResult();
}

VisitorResult SageCompiler::build_function_with_block(string function_name) {
    builder.new_frame(function_name);
    builder.build_instruction(OP_LABEL, get_procedure_frame_id(function_name));
    return VisitorResult();
}

VisitorResult SageCompiler::build_alloca(symbol_entry *var_symbol) {
    if (var_symbol->spilled) {
        builder.build_instruction(OP_ADD, STACK_POINTER, STACK_POINTER, 1);
    }

    return VisitorResult();
}

VisitorResult SageCompiler::build_operator(VisitorResult value1, VisitorResult value2, SageOpCode opcode) {
    int result_register = get_volatile();
    int control_path_id = ((int)value1.get_result_state(&symbol_table) * 4) + (int)value2.get_result_state(&symbol_table);
    switch (control_path_id) {
        case 0:
            builder.build_instruction(opcode, result_register, value1.immediate_value, value2.immediate_value);
            break;
        case 1: {
            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);
            int temporary_result = get_volatile();
            builder.build_instruction(OP_LOAD, temporary_result, value2_entry->spill_offset);
            builder.build_instruction(opcode, result_register, value1.immediate_value, temporary_result);
            break;
        }
        case 2: {
            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);
            builder.build_instruction(opcode, result_register, value1.immediate_value, value2_entry->assigned_register);
            break;
        }
        case 3: {
            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);
            builder.build_instruction(opcode, result_register, value1.immediate_value, value2_entry->value);
            break;
        }
        case 4: {
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);
            int temporary_result = get_volatile();
            builder.build_instruction(OP_LOAD, temporary_result, value1_entry->spill_offset);
            builder.build_instruction(opcode, result_register, temporary_result, value2.immediate_value);
            break;
        }
        case 5: {
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);
            int temporary_result1 = get_volatile();
            builder.build_instruction(OP_LOAD, temporary_result1, value1_entry->spill_offset);

            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);
            int temporary_result2 = get_volatile();
            builder.build_instruction(OP_LOAD, temporary_result2, value2_entry->spill_offset);

            builder.build_instruction(opcode, result_register, temporary_result1, temporary_result2);
            break;
        }
        case 6: {
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);
            int temporary_result1 = get_volatile();
            builder.build_instruction(OP_LOAD, temporary_result1, value1_entry->spill_offset);

            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);
            builder.build_instruction(opcode, result_register, temporary_result1, value2_entry->assigned_register);
            break;
        }
        case 7: {
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);
            int temporary_result1 = get_volatile();
            builder.build_instruction(OP_LOAD, temporary_result1, value1_entry->spill_offset);

            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);
            builder.build_instruction(opcode, result_register, temporary_result1, value2_entry->value);
            break;
        }
        case 8: {
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);

            builder.build_instruction(opcode, result_register, value1_entry->assigned_register, value2.immediate_value);
            break;
        }
        case 9: {
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);

            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);
            int temporary_result2 = get_volatile();
            builder.build_instruction(OP_LOAD, temporary_result2, value2_entry->spill_offset);

            builder.build_instruction(opcode, result_register, value1_entry->assigned_register, temporary_result2);
            break;
        }
        case 10: {
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);
            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);

            builder.build_instruction(opcode, result_register, value1_entry->assigned_register, value2_entry->assigned_register);
            break;
        }
        case 11: {
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);
            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);

            builder.build_instruction(opcode, result_register, value1_entry->assigned_register, value2_entry->value);
            break;
        }
        case 12: {
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);

            builder.build_instruction(opcode, result_register, value1_entry->value, value2.immediate_value);
            break;
        }
        case 13: {
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);
            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);
            int temporary_result2 = get_volatile();
            builder.build_instruction(OP_LOAD, temporary_result2, value2_entry->spill_offset);

            builder.build_instruction(opcode, result_register, value1_entry->value, temporary_result2);
            break;
        }
        case 14: {
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);
            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);

            builder.build_instruction(opcode, result_register, value1_entry->value, value2_entry->assigned_register);
            break;
        }
        case 15: {
            symbol_entry *value1_entry = symbol_table.lookup_by_index(value1.symbol_table_index);
            symbol_entry *value2_entry = symbol_table.lookup_by_index(value2.symbol_table_index);

            builder.build_instruction(opcode, result_register, value1_entry->value, value2_entry->value);
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
    return build_operator(value1, value2, OP_ADD);
}

VisitorResult SageCompiler::build_sub(VisitorResult value1, VisitorResult value2) {
    return build_operator(value1, value2, OP_SUB);
}

VisitorResult SageCompiler::build_mul(VisitorResult value1, VisitorResult value2) {
    return build_operator(value1, value2, OP_MUL);
}

VisitorResult SageCompiler::build_div(VisitorResult value1, VisitorResult value2) {
    return build_operator(value1, value2, OP_DIV);
}

VisitorResult SageCompiler::build_and(VisitorResult value1, VisitorResult value2) {
    return build_operator(value1, value2, OP_AND);
}

VisitorResult SageCompiler::build_or(VisitorResult value1, VisitorResult value2) {
    return build_operator(value1, value2, OP_OR);
}

VisitorResult SageCompiler::build_load(NodeIndex reference_node) {
    string reference_name = node_manager->get_identifier(reference_node);
    int scope_id = node_manager->get_scope_id(reference_node);
    auto symbol = symbol_table.lookup(reference_name, scope_id);

    if (symbol->spilled) {
        int volatile_reg = get_volatile();
        builder.build_instruction(OP_LOAD, volatile_reg, symbol->spill_offset);
        symbol->assigned_register = volatile_reg;
    }

    return symbol_table.lookup_table_index(reference_name, scope_id);
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
