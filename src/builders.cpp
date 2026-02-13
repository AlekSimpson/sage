#include <atomic>
#include <cassert>
#include <complex>

#include "../include/platform.h"
#include <cstring>
#include <boost/preprocessor/arithmetic/add.hpp>

#include "../include/bytecode_builder.h"
#include "../include/symbols.h"
#include "../include/interpreter.h"
#include "../include/node_manager.h"
#include "../include/codegen.h"
#include "../include/sage_bytecode.h"

std::unordered_map<std::pair<CanonicalType, int>, std::unique_ptr<SageType> > TypeRegistery::builtin_types;
std::unordered_map<SageType *, std::unique_ptr<SageType> > TypeRegistery::pointer_types;
std::unordered_map<std::pair<SageType *, int>, std::unique_ptr<SageType> > TypeRegistery::array_types;
std::unordered_map<std::pair<SageType *, int>, std::unique_ptr<SageType> > TypeRegistery::dyn_array_types;
std::unordered_map<std::pair<SageType *, int>, std::unique_ptr<SageType> > TypeRegistery::reference_types;
std::unordered_map<std::pair<std::vector<SageType *>, std::vector<SageType *> >, std::unique_ptr<SageType> >
TypeRegistery::function_types;
std::unordered_map<std::string, std::unique_ptr<SageType> > TypeRegistery::struct_types;

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
    } else {
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
    result.push_back(Command(VOP_EXIT, -1, _00));
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

void BytecodeBuilder::build_builtin_instruction(SageOpCode opcode, int operand1, int operand2, int operand3,
                                                AddressMode mode) {
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

void BytecodeBuilder::build_fmove_immediate(int destination_register, double immediate_value) {
    int double_bitcast_value;
    std::memcpy(&double_bitcast_value, &immediate_value, sizeof(double_bitcast_value));

    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        OP_FMOV,
        destination_register,
        double_bitcast_value,
        _00
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

void BytecodeBuilder::build_int_to_float_move_register(int dest_float_register, int src_int_register) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        OP_ITF_MOV,
        dest_float_register,
        src_int_register,
        _01
    ));
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
    build_builtin_instruction(OP_MOV, 10, 1, _01); // save digit count (r1) in temp register r10
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
    build_builtin_instruction(OP_MOV, 10, 1, _01); // save digit count (r1) in temp register r10
    build_builtin_instruction(OP_MOV, 1, 0, _01); // save character buff pointer to print into r1
    build_builtin_instruction(OP_MOV, 2, 10, _01); // save digit count into r2
    build_builtin_instruction(OP_MOV, 0, STDOUT_FILENO, _00); // tell system that this is outputting to stdout
    build_builtin_instruction(OP_SYSCALL, -1, _00);
    build_builtin_instruction(OP_RET, -1, _00);
    runtime_procedure_stack.pop();
    comptime_procedure_stack.pop();
}

VisitorResult SageCompiler::build_store(VisitorResult right_value, symbol_entry *var_symbol) {
    // var_symbol in register ==> LOAD/MOVE
    // var_symbol on stack    ==> STORE
    auto variable_result = VisitorResult(symbol_table, var_symbol->symbol_id);
    switch (variable_result.state) {
        case VisitorResultState::SPILLED: {
            right_value.to_stack_instruction(*this, var_symbol->spill_offset, var_symbol->type);
            break;
        }
        case VisitorResultState::REGISTER: {
            right_value.to_register_instruction(*this, var_symbol->assigned_register, var_symbol->type);
            break;
        }
        default:
            ErrorLogger::get().log_internal_error_unsafe("builders.cpp", current_linenum,
                                                         "Attempted to build invalid store instruction. Can only store to variable symbols which are in registers or on the stack.");
    }
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
    return one.result_type->identify() == FLOAT || two.result_type->identify() == FLOAT;
}

VisitorResult SageCompiler::build_operator(
    VisitorResult value1, VisitorResult value2, SageOpCode opcode) {
    AddressMode mode = _11;
    int result_register = get_volatile_register();
    auto material_result1 = value1.materialize_register(*this);
    int register1 = material_result1.first;
    bool one_is_immediate = material_result1.second;

    auto material2 = value2.materialize_register(*this);
    int register2 = material2.first;
    bool two_is_immediate = material2.second;

    if (one_is_immediate && two_is_immediate) {
        register1 = value1.immediate_value;
        register2 = value2.immediate_value;
        mode = _00;
    } else if (one_is_immediate) {
        register1 = value1.immediate_value;
        mode = _01;
    } else if (two_is_immediate) {
        register2 = value2.immediate_value;
        mode = _10;
    }

    SageType *temporary_result_type = value1.result_type;
    if (value1.result_type->identify() == FLOAT || value2.result_type->identify() == FLOAT) {
        int bytesize = std::max(value1.result_type->size, value2.result_type->size);
        temporary_result_type = TR::get_float_type(bytesize);
    }

    builder.build_instruction(opcode, result_register, register1, register2, mode);
    return VisitorResult(result_register, temporary_result_type, true);
}

VisitorResult SageCompiler::build_add(VisitorResult value1, VisitorResult value2) {
    bool is_float = is_float_operation(value1, value2);
    return build_operator(value1, value2, is_float ? OP_FADD : OP_ADD);
}

VisitorResult SageCompiler::build_sub(VisitorResult value1, VisitorResult value2) {
    bool is_float = is_float_operation(value1, value2);
    return build_operator(value1, value2, is_float ? OP_FSUB : OP_SUB);
}

VisitorResult SageCompiler::build_mul(VisitorResult value1, VisitorResult value2) {
    bool is_float = is_float_operation(value1, value2);
    return build_operator(value1, value2, is_float ? OP_FMUL : OP_MUL);
}

VisitorResult SageCompiler::build_div(VisitorResult value1, VisitorResult value2) {
    bool is_float = is_float_operation(value1, value2);
    return build_operator(value1, value2, is_float ? OP_FDIV : OP_DIV);
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
    VisitorResult load_result = VisitorResult(symbol_table, symbol->symbol_id);
    return load_result;
}


void VisitorResult::to_register_instruction(SageCompiler &compiler, int argument_register,
                                            SageType *argument_type) {
    // generate instructions which stores this visitor result into an argument register
    auto &builder = compiler.builder;
    auto &symbol_table = compiler.symbol_table;

    switch (state) {
        case VisitorResultState::IMMEDIATE: {
            if (argument_type->identify() == FLOAT) {
                builder.build_fmove_immediate(argument_register, immediate_value);
            } else {
                builder.build_move_immediate(argument_register, immediate_value);
            }
            break;
        }
        case VisitorResultState::SPILLED: {
            auto *entry = symbol_table.lookup_by_index(symbol_table_index);
            if (argument_type->identify() == FLOAT) {
                // moving value from stack into float register
                builder.build_fload(argument_register, entry->spill_offset);
            } else {
                // moving value from stack into normal register
                builder.build_load(argument_register, entry->spill_offset);
            }
            break;
        }
        case VisitorResultState::REGISTER: {
            auto *entry = symbol_table.lookup_by_index(symbol_table_index);
            int combination = (((int) argument_type->identify() == FLOAT) * 2) + (int) entry->type->identify() == FLOAT;

            switch (combination) {
                case 0: /* arg=int, vis=int */
                    builder.build_move_register(argument_register, entry->assigned_register);
                    break;
                case 1: /* arg=int, vis=float */
                    //builder.build_move_register(argument_register, );
                    compiler.logger.log_internal_error_unsafe("builders.cpp", current_linenum,
                                                              "This visitor combination shouldn't happen.");
                    break;
                case 2: /* arg=float, vis=int */
                    builder.build_int_to_float_move_register(argument_register, entry->assigned_register);
                    break;
                case 4: /* arg=float, vis=float */
                    builder.build_fmove_register(argument_register, entry->assigned_register);
                    break;
                default:
                    break;
            }
            break;
        }
        case VisitorResultState::VALUE: {
            auto *entry = symbol_table.lookup_by_index(symbol_table_index);
            int static_pointer = compiler.get_literal_static_pointer(symbol_table_index);
            if (argument_type->is_array() || argument_type->is_pointer() || argument_type->is_struct()) {
                builder.build_move_immediate(argument_register, static_pointer);
                break;
            }

            if (argument_type->identify() == FLOAT) {
                // we are either moving a static literal or an immediate value into a float register
                if (static_pointer == -1) {
                    int temporary_register = compiler.get_volatile_register();
                    builder.build_move_immediate(temporary_register, entry->value);
                    builder.build_int_to_float_move_register(argument_register, temporary_register);
                    break;
                }
                builder.build_fload(argument_register, static_pointer);
            } else {
                // we are either moving a static literal or an immediate value into a register
                if (static_pointer == -1) {
                    builder.build_move_immediate(argument_register, entry->value);
                    break;
                }
                builder.build_load(argument_register, static_pointer);
            }
            break;
        }
        case VisitorResultState::TEMP_REGISTER: {
            auto opcode = result_type->identify() == FLOAT ? OP_FMOV : OP_MOV;
            builder.build_instruction(opcode, argument_register, temporary_result_register, _01);
            break;
        }
        case VisitorResultState::LIST: {
            // TODO
            break;
        }
    }
}

void VisitorResult::to_stack_instruction(SageCompiler &compiler, int offset, SageType *argument_type,
                                         AddressMode offset_mode) {
    // generate instructions which stores this visitor result into an argument register
    auto &builder = compiler.builder;
    auto &symbol_table = compiler.symbol_table;
    auto *entry = symbol_table.lookup_by_index(symbol_table_index);

    map<int, SageOpCode> register_state_combination_map = {
        {0, OP_STORE},
        {1, OP_FSTORE},
        {2, OP_STORE},
        {3, OP_FSTORE}
    };

    switch (state) {
        case VisitorResultState::IMMEDIATE: {
            int temporary_register = immediate_value; // if its not a float then we can just directly store the immediate int value
            auto opcode = OP_STORE;
            if (result_type->identify() == FLOAT) {
                temporary_register = compiler.get_volatile_register();
                builder.build_fmove_immediate(temporary_register, immediate_value);
                opcode = OP_FSTORE;
            }
            builder.build_instruction(opcode, offset, temporary_register, offset_mode);
            break;
        }
        case VisitorResultState::SPILLED: {
            builder.build_instruction(OP_MEMCPY, offset, entry->spill_offset, offset_mode);
            break;
        }
        case VisitorResultState::REGISTER: {
            int type_state_combination = (((int) argument_type->identify() == FLOAT) * 2) + (int) entry->type->
                                         identify() == FLOAT;
            auto opcode = register_state_combination_map[type_state_combination];
            AddressMode argument_mode = _01;

            builder.build_instruction(opcode, offset, entry->assigned_register, offset_mode + argument_mode);
            break;
        }
        case VisitorResultState::VALUE: {
            int static_pointer = compiler.get_literal_static_pointer(symbol_table_index);
            if (static_pointer == -1) {
                builder.build_instruction(OP_STORE, offset, entry->value, offset_mode);
                break;
            }
            builder.build_instruction(OP_MEMCPY, offset, static_pointer, offset_mode);
            break;
        }
        case VisitorResultState::TEMP_REGISTER: {
            auto opcode = result_type->identify() == FLOAT ? OP_FSTORE : OP_STORE;
            AddressMode argument_mode = _01;
            builder.build_instruction(opcode, offset, temporary_result_register, offset_mode + argument_mode);
            break;
        }
        case VisitorResultState::LIST: {
            // TODO
            break;
        }
    }
}

pair<int, bool> VisitorResult::materialize_register(SageCompiler &compiler) {
    auto &builder = compiler.builder;
    auto *entry = compiler.symbol_table.lookup_by_index(symbol_table_index);

    auto opcode = entry->type->identify() == FLOAT ? OP_FLOAD : OP_LOAD;

    switch (state) {
        case VisitorResultState::IMMEDIATE: {
            if (result_type->identify() == FLOAT) {
                int output_register = compiler.get_volatile_register();
                builder.build_fmove_immediate(output_register, entry->value);
                return make_pair<int, bool>(std::move(output_register), false);
            }
            return make_pair<int, bool>(immediate_value, true);
        }
        case VisitorResultState::SPILLED: {
            int output_register = compiler.get_volatile_register();
            builder.build_instruction(opcode, output_register, entry->spill_offset, _00);
            return make_pair<int, bool>(std::move(output_register), false);
        }
        case VisitorResultState::REGISTER: {
            return make_pair<int, bool>(std::move(entry->assigned_register), false);
        }
        case VisitorResultState::VALUE: {
            int output_register = compiler.get_volatile_register();
            int static_pointer = compiler.get_literal_static_pointer(symbol_table_index);
            if (entry->type->is_array() || entry->type->is_pointer() || entry->type->is_struct()) {
                return make_pair<int, bool>(std::move(static_pointer), true);
            }
            builder.build_instruction(opcode, output_register, static_pointer, _00);
            return make_pair<int, bool>(std::move(output_register), false);
        }
        case VisitorResultState::TEMP_REGISTER: {
            return make_pair<int, bool>(std::move(temporary_result_register), false);
        }
        case VisitorResultState::LIST: {
            // TODO:
            return make_pair<int, bool>(std::move(immediate_value), true);
        }
        default:
            return make_pair<int, bool>(std::move(immediate_value), true);
    }
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
