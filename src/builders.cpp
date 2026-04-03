#include <atomic>
#include <cassert>
#include <complex>

#include "../include/platform.h"
#include <cstring>

#include "../include/bytecode_builder.h"
#include "../include/symbols.h"
#include "../include/interpreter.h"
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
    // DJB2 hash can exceed INT_MAX for some names; mask the sign bit so procedure
    // IDs are always non-negative and display correctly.
    return static_cast<int>(hash & 0x7FFFFFFF);
}

BytecodeBuilder::BytecodeBuilder() {
    int global_id = get_procedure_frame_id(GLOBAL_NAME);
    comptime_procedures[global_id] = ProcedureFrame(GLOBAL_NAME);
    comptime_procedure_stack.push(global_id);

    runtime_procedures[global_id] = ProcedureFrame(GLOBAL_NAME);
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

    int global_id = get_procedure_frame_id(GLOBAL_NAME);
    procedures[global_id] = ProcedureFrame(GLOBAL_NAME);
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
        if (procedure_name == GLOBAL_NAME) continue;

        bytecode current_instructions = frame.procedure_instructions;
        procedure_line_locations[id] = result.size();

        result.insert(result.end(), current_instructions.begin(), current_instructions.end());
    }

    int global_id = get_procedure_frame_id(GLOBAL_NAME);
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
        if (procedure_name == GLOBAL_NAME) continue;

        bytecode current_instructions = frame.procedure_instructions;
        procedure_line_locations[id] = result.size();

        result.insert(result.end(), current_instructions.begin(), current_instructions.end());
    }

    int global_id = get_procedure_frame_id(GLOBAL_NAME);
    auto global_instructions = runtime_procedures[global_id].procedure_instructions;

    // Global initialization code always runs first
    procedure_line_locations[global_id] = result.size();
    result.insert(result.end(), global_instructions.begin(), global_instructions.end());

    if (runtime_has_main_function) {
        // After global init, call main
        int main_id = get_procedure_frame_id("main");
        result.push_back(Command(OP_CALL, main_id, _00));
    }

    result.push_back(Command(VOP_EXIT, -1, _00));
    return result;
}

void BytecodeBuilder::build_instruction(SageOpCode opcode, int64_t operand1, int64_t operand2, int64_t operand3,
                                        AddressMode mode) {
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

void BytecodeBuilder::build_instruction(SageOpCode opcode, int64_t operand1, int64_t operand2, AddressMode mode) {
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

void BytecodeBuilder::build_instruction(SageOpCode opcode, int64_t operand, AddressMode mode) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        opcode,
        operand,
        mode
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_builtin_instruction(SageOpCode opcode, int64_t operand1, int64_t operand2, int64_t operand3,
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

void BytecodeBuilder::build_builtin_instruction(SageOpCode opcode, int64_t operand1, int64_t operand2,
                                                AddressMode mode) {
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

void BytecodeBuilder::build_builtin_instruction(SageOpCode opcode, int64_t operand, AddressMode mode) {
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

void BytecodeBuilder::build_load(int sage_register, int offset, int bytes) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        OP_LOAD,
        bytes,
        sage_register,
        offset,
        _00
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_store_immediate(int offset, int64_t immediate, int bytes) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        OP_STORE,
        bytes,
        offset,
        immediate,
        _00
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_store_register(int offset, int sage_register, int bytes) {
    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        OP_STORE,
        bytes,
        offset,
        sage_register,
        _01
    ));
    increment_total_instruction_count(1);
}

void BytecodeBuilder::build_fmove_immediate(int destination_register, int64_t immediate_value) {
    double debugvalue;
    std::memcpy(&debugvalue, &immediate_value, sizeof(double));

    auto &procedures = get_active_procedures();
    auto &procedure_stack = get_active_procedure_stack();

    procedures[procedure_stack.top()].procedure_instructions.push_back(Command(
        OP_FMOV,
        destination_register,
        immediate_value,
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

void BytecodeBuilder::build_move_immediate(int sage_register, int64_t immediate) {
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

VisitorResult SageCompiler::build_store(VisitorResult right_value, SymbolEntry *var_symbol) {
    // var_symbol in register ==> LOAD/MOVE
    // var_symbol on stack    ==> STORE
    auto variable_result = VisitorResult(symbol_table, var_symbol->symbol_index);
    switch (variable_result.state) {
        case VisitorResultState::VALUE:
        case VisitorResultState::SPILLED: {
            right_value.to_stack_instruction(*this, var_symbol->stack_offset);
            break;
        }
        case VisitorResultState::REGISTER: {
            right_value.to_register_instruction(*this, var_symbol->assigned_register, var_symbol->datatype);
            break;
        }
        default:
            assertm(false, "Attempted to build invalid store instruction. Can only store to variable symbols which are in registers or on the stack.");
    }
    return VisitorResult();
}

VisitorResult SageCompiler::build_function_with_block(string function_name) {
    builder.new_frame(function_name);
    builder.build_instruction(OP_LABEL, get_procedure_frame_id(function_name), _00);
    return VisitorResult();
}

void SageCompiler::build_alloca(SymbolEntry *var_symbol) {
    if (!var_symbol->spilled) return;

    auto datatype_identity = var_symbol->datatype->identify();
    if (datatype_identity == ARRAY ||
        datatype_identity == DYN_ARRAY) {

        int array_struct_start = get_volatile_register();
        int array_struct_length_address = get_volatile_register();
        int array_memory_start = get_volatile_register();
        builder.build_move_register(array_struct_start, STACK_POINTER);
        builder.build_move_register(array_struct_length_address, STACK_POINTER);
        builder.build_instruction(OP_SUB, array_struct_length_address, STACK_POINTER, 8, _10);

        int array_byte_size = ((SageArrayType *)var_symbol->datatype)->array_size;
        int array_length = ((SageArrayType *)var_symbol->datatype)->length;
        int offset = array_byte_size + var_symbol->datatype->size;
        // decrement SP first so array_memory_start captures new_SP (= first element address)
        builder.build_instruction(OP_SUB, STACK_POINTER, STACK_POINTER, offset, _10);
        builder.build_move_register(array_memory_start, STACK_POINTER);

        builder.build_instruction(OP_STOREA, 8, array_struct_start, array_memory_start, _11);
        builder.build_instruction(OP_STOREA, 8, array_struct_length_address, array_length, _10);

        return;
    }

    int offset = var_symbol->datatype->size;
    builder.build_instruction(OP_SUB, STACK_POINTER, STACK_POINTER, offset, _10);
}

bool SageCompiler::is_float_operation(VisitorResult &one, VisitorResult &two) {
    return one.result_type->identify() == FLOAT || two.result_type->identify() == FLOAT;
}

VisitorResult SageCompiler::build_operator(
    VisitorResult value1, VisitorResult value2, SageOpCode opcode) {
    AddressMode mode = _11;
    int result_register = get_volatile_register();
    auto material_result1 = value1.materialize_register(*this);
    int64_t register1 = material_result1.first;
    bool one_is_immediate = material_result1.second;

    auto material2 = value2.materialize_register(*this);
    int64_t register2 = material2.first;
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
            if (entry->datatype->size > 8) {
                if (entry->datatype->match(TR::get_string_type())) {
                    // Strings store a data pointer in their first field; load it so the callee
                    // receives the actual buffer address rather than a raw frame offset.
                    builder.build_load(argument_register, entry->stack_offset, 8);
                } else {
                    // can't fit raw value in register so just move pointer into register
                    builder.build_move_immediate(argument_register, entry->stack_offset);
                }
            } else {
                builder.build_load(argument_register, entry->stack_offset, 8);
            }
            break;
        }
        case VisitorResultState::REGISTER: {
            auto *entry = symbol_table.lookup_by_index(symbol_table_index);
            int combination = (((int) argument_type->identify() == FLOAT) * 2) + (int) entry->datatype->identify() ==
                              FLOAT;

            switch (combination) {
                case 0: /* arg=int, vis=int */
                    builder.build_move_register(argument_register, entry->assigned_register);
                    break;
                case 2: /* arg=float, vis=int */
                    builder.build_int_to_float_move_register(argument_register, entry->assigned_register);
                    break;
                case 4: /* arg=float, vis=float */
                    builder.build_fmove_register(argument_register, entry->assigned_register);
                    break;
                default:
                    assertm(false, "This visitor combination shouldn't happen. Type checking failed somehow probably.");
            }
            break;
        }
        case VisitorResultState::VALUE: {
            int static_pointer = compiler.get_literal_static_pointer(symbol_table_index);
            builder.build_move_immediate(argument_register, static_pointer);
            break;
        }
        case VisitorResultState::TEMP_REGISTER: {
            auto opcode = result_type->identify() == FLOAT ? OP_FMOV : OP_MOV;
            builder.build_instruction(opcode, argument_register, temporary_result_register, _01);
            break;
        }
    }
}

void VisitorResult::to_stack_instruction_absolute(SageCompiler &compiler, int absolute_address, bool ascending_memory, AddressMode address_mode) {
    auto &builder = compiler.builder;
    auto &symbol_table = compiler.symbol_table;
    auto *visitor_result_entry = symbol_table.lookup_by_index(symbol_table_index);
    assertm(visitor_result_entry != nullptr, "symbol entry was nullptr");

    switch (state) {
        case VisitorResultState::IMMEDIATE: {
            int store_size = result_type != nullptr ? result_type->size : 8;
            if (result_type != nullptr && result_type->identify() == FLOAT) {
                builder.build_fmove_immediate(compiler.get_volatile_register(), immediate_value);
            }
            builder.build_instruction(OP_STOREA, store_size, absolute_address, immediate_value, address_mode);
            break;
        }
        case VisitorResultState::SPILLED: {
            //int src_address_reg = compiler.get_volatile_register();
            //int dest_address_reg = compiler.get_volatile_register();
            //int size = visitor_result_entry->datatype->size;

            // note: we need to think about lowest and highest address because std::memcpy copies up instead of down like our stack works
            // source: lowest address of the source struct
            // builder.build_instruction(OP_SUB, src_address_reg, 24, visitor_result_entry->stack_offset + size, _10);

            // // destination: lowest address of the dest struct
            // builder.build_instruction(OP_SUB, dest_address_reg, absolute_address, size, _10);

            // source: the struct starts at fp - stack_offset, data goes UPWARD from there
            //builder.build_instruction(OP_SUB, src_address_reg, 24, visitor_result_entry->stack_offset, _10);

            //// destination: starts at the absolute_address register value
            //builder.build_instruction(OP_MOV, dest_address_reg, absolute_address, _01);

            //builder.build_instruction(OP_ADDR_MEMCPY, size, dest_address_reg, src_address_reg, _11);
            int src_address_reg = compiler.get_volatile_register();
            int dest_address_reg = compiler.get_volatile_register();
            int size = visitor_result_entry->datatype->size;
            int lowest_address_adjustment = size - 8;

            builder.build_instruction(OP_SUB, src_address_reg, 24, visitor_result_entry->stack_offset + lowest_address_adjustment, _10);
            builder.build_instruction(OP_SUB, dest_address_reg, absolute_address, lowest_address_adjustment, _10);
            builder.build_instruction(OP_ADDR_MEMCPY, size, dest_address_reg, src_address_reg, _11);
            break;
        }
        case VisitorResultState::REGISTER: {
            builder.build_instruction(OP_STOREA, visitor_result_entry->datatype->size, absolute_address, visitor_result_entry->assigned_register,
                                      address_mode + _01);
            break;
        }
        case VisitorResultState::VALUE: {
            int static_pointer = visitor_result_entry->static_pointer;

            if (visitor_result_entry->datatype->match(TR::get_string_type())) {
                // value is a string
                int64_t byte_count = 0;
                memcpy(&byte_count, visitor_result_entry->data.byte_data + 8, 8); // get string length

                auto temp_pointer_register = compiler.get_volatile_register();
                builder.build_instruction(OP_SUB, STACK_POINTER, STACK_POINTER, byte_count, _10);
                builder.build_move_register(temp_pointer_register, STACK_POINTER);

                // store first 8 bytes (pointer) at offset
                builder.build_instruction(OP_STOREA, 8, absolute_address, temp_pointer_register, address_mode + _01);

                builder.build_instruction(OP_STATIC_COPY, byte_count, temp_pointer_register, static_pointer, _10);

                // store second 8 bytes (length) at offset + 8
                if (address_mode == _10) {
                    SageOpCode pointer_arithmetic_op = ascending_memory ? OP_ADD : OP_SUB;
                    int temp_reg = compiler.get_volatile_register();
                    builder.build_instruction(pointer_arithmetic_op, temp_reg, absolute_address, 8, _10);
                    builder.build_instruction(OP_STOREA, 8, temp_reg, byte_count, _10);
                } else {
                    builder.build_instruction(OP_STOREA, 8, absolute_address + 8, byte_count, _00);
                }

            }else if (visitor_result_entry->datatype->identify() == ARRAY) {
                // value is a static array
                SageArrayType *_datatype = (SageArrayType *)visitor_result_entry->datatype;
                int64_t inner_type_size = _datatype->array_type->size;
                int64_t string_length = _datatype->length;
                int64_t byte_count = string_length * inner_type_size;

                // load the first_ptr (absolute element area address) from the struct's first field
                auto temp_pointer_register = compiler.get_volatile_register();
                builder.build_instruction(OP_LOADA, 8, temp_pointer_register, absolute_address, _01);

                int src_reg = compiler.get_volatile_register();
                builder.build_move_immediate(src_reg, static_pointer);
                builder.build_instruction(OP_ADDR_MEMCPY, byte_count, temp_pointer_register, src_reg, _11);
            }
            break;
        }
        case VisitorResultState::TEMP_REGISTER: {
            auto entry_type = visitor_result_entry->datatype->expression_resolution_type();
            if (entry_type->is_struct()) {
                int size = entry_type->size;

                if (size <= 8) {
                    // Small struct: the value was returned directly in the register (not via a stack slot pointer).
                    builder.build_instruction(OP_STOREA, size, absolute_address, temporary_result_register, address_mode + _01);
                } else {
                    // Large struct: the register holds a pointer to the return slot on the stack.
                    if (ascending_memory) {
                        // The return slot uses stack convention: field 0 at [r6], field 1 at [r6-8], etc.
                        // Ascending memory (array element) expects field 0 at [base+0], field 1 at [base+8], etc.
                        // These layouts are reversed, so copy each field individually.
                        SageStructType *struct_type = dynamic_cast<SageStructType *>(entry_type);
                        int ascending_offset = 0;
                        for (SageType *field_type : struct_type->member_types) {
                            int field_size = field_type->size;
                            int src_reg = compiler.get_volatile_register();
                            int dest_reg = compiler.get_volatile_register();
                            builder.build_instruction(OP_SUB, src_reg, temporary_result_register, ascending_offset, _10);
                            builder.build_instruction(OP_ADD, dest_reg, absolute_address, ascending_offset, _10);
                            builder.build_instruction(OP_ADDR_MEMCPY, field_size, dest_reg, src_reg, _11);
                            ascending_offset += field_size;
                        }
                    } else {
                        int lowest_address_adjustment = size - 8;
                        int dest_address_reg = compiler.get_volatile_register();
                        int src_address_reg = compiler.get_volatile_register();
                        builder.build_instruction(OP_SUB, dest_address_reg, absolute_address, lowest_address_adjustment, _10);
                        builder.build_instruction(OP_SUB, src_address_reg, temporary_result_register, lowest_address_adjustment, _10);
                        builder.build_instruction(OP_ADDR_MEMCPY, size, dest_address_reg, src_address_reg, _11);
                    }
                }
            } else if (entry_type->identify() == ARRAY || entry_type->identify() == DYN_ARRAY) {
                SageArrayType *array_type = (SageArrayType *)entry_type;
                int element_byte_count = array_type->array_size;

                // load destination element pointer (from dest struct at absolute_address)
                int dest_element_ptr = compiler.get_volatile_register();
                builder.build_instruction(OP_LOADA, 8, dest_element_ptr, absolute_address, _01);

                // load source element pointer (from return slot struct at temporary_result_register)
                int src_element_ptr = compiler.get_volatile_register();
                builder.build_instruction(OP_LOADA, 8, src_element_ptr, temporary_result_register, _01);

                builder.build_instruction(OP_ADDR_MEMCPY, element_byte_count, dest_element_ptr, src_element_ptr, _11);
            } else {
                builder.build_instruction(OP_STOREA, visitor_result_entry->datatype->size, absolute_address, temporary_result_register,
                                          address_mode + _01);
            }
            break;
        }
    }
}

void VisitorResult::to_stack_instruction(SageCompiler &compiler, int offset, AddressMode offset_mode) {
    auto &builder = compiler.builder;
    auto &symbol_table = compiler.symbol_table;
    auto *visitor_result_entry = symbol_table.lookup_by_index(symbol_table_index);
    assertm(visitor_result_entry != nullptr, "symbol entry was nullptr");

    switch (state) {
        case VisitorResultState::IMMEDIATE: {
            int store_size = result_type != nullptr ? result_type->size : 8;
            if (result_type != nullptr && result_type->identify() == FLOAT) {
                builder.build_fmove_immediate(compiler.get_volatile_register(), immediate_value);
            }
            builder.build_instruction(OP_STORE, store_size, offset, immediate_value, offset_mode);
            break;
        }
        case VisitorResultState::SPILLED: {
            builder.build_instruction(OP_MEMCPY, visitor_result_entry->datatype->size, offset, visitor_result_entry->stack_offset, offset_mode);
            break;
        }
        case VisitorResultState::REGISTER: {
            builder.build_instruction(OP_STORE, visitor_result_entry->datatype->size, offset, visitor_result_entry->assigned_register,
                                      offset_mode + _01);
            break;
        }
        case VisitorResultState::VALUE: {
            int static_pointer = compiler.get_literal_static_pointer(symbol_table_index);

           if (visitor_result_entry->datatype->match(TR::get_string_type())) {
                // value is a string
                int64_t byte_count = 0;
                memcpy(&byte_count, visitor_result_entry->data.byte_data + 8, 8); // get string length
                int64_t string_length = byte_count;

                auto temp_pointer_register = compiler.get_volatile_register();
                builder.build_instruction(OP_SUB, STACK_POINTER, STACK_POINTER, byte_count, _10);
                builder.build_move_register(temp_pointer_register, STACK_POINTER);

                // store first 8 bytes (pointer) at offset
                builder.build_instruction(OP_STORE, 8, offset, temp_pointer_register, offset_mode + _01);

                builder.build_instruction(OP_STATIC_COPY, byte_count, temp_pointer_register, static_pointer, _10);

                // store second 8 bytes (length) at offset + 8
                if (offset_mode == _10) {
                    int temp_reg = compiler.get_volatile_register();
                    builder.build_instruction(OP_ADD, temp_reg, offset, 8, _10);
                    builder.build_instruction(OP_STOREA, 8, temp_reg, string_length, _10);
                } else {
                    builder.build_instruction(OP_STORE, 8, offset + 8, string_length, _00);
                }

            }else if (visitor_result_entry->datatype->identify() == ARRAY) {
                // value is a static array
                SageArrayType *_datatype = (SageArrayType *)visitor_result_entry->datatype;
                int64_t inner_type_size = _datatype->array_type->size;
                int64_t string_length = _datatype->length;
                int64_t byte_count = string_length * inner_type_size;

                // load the first_ptr (absolute element area address) from the struct's first field
                auto temp_pointer_register = compiler.get_volatile_register();
                if (offset_mode == _10) {
                    builder.build_instruction(OP_LOAD, 8, temp_pointer_register, offset, _01);
                }else {
                    builder.build_instruction(OP_LOAD, 8, temp_pointer_register, offset, _00);
                }

                int source_register = compiler.get_volatile_register();
                builder.build_move_immediate(source_register, static_pointer);
                builder.build_instruction(OP_ADDR_MEMCPY, byte_count, temp_pointer_register, source_register, _11);
            }
            break;
        }
        case VisitorResultState::TEMP_REGISTER: {
            auto entry_type = visitor_result_entry->datatype->expression_resolution_type();
            if (entry_type->is_struct()) {
                int size = entry_type->size;
                int lowest_address_adjustment = size - 8;

                int dest_address_reg = compiler.get_volatile_register();
                int src_address_reg = compiler.get_volatile_register();

                // dest: fp - (offset + size - 8)
                builder.build_instruction(OP_SUB, dest_address_reg, 24, offset + lowest_address_adjustment, _10);
                // src: temporary_result_register - (size - 8)
                builder.build_instruction(OP_SUB, src_address_reg, temporary_result_register, lowest_address_adjustment, _10);
                builder.build_instruction(OP_ADDR_MEMCPY, size, dest_address_reg, src_address_reg, _11);
            }else if (entry_type->identify() == ARRAY || entry_type->identify() == DYN_ARRAY) {
                SageArrayType *array_type = (SageArrayType *)entry_type;
                int element_byte_count = array_type->array_size;

                // load destination element pointer
                int dest_struct_addr = compiler.get_volatile_register();
                int dest_element_ptr = compiler.get_volatile_register();
                builder.build_instruction(OP_SUB, dest_struct_addr, 24, offset, _10);
                builder.build_instruction(OP_LOADA, 8, dest_element_ptr, dest_struct_addr, _01);

                // load source element pointer (from return slot struct at temporary_result_register)
                int src_element_ptr = compiler.get_volatile_register();
                builder.build_instruction(OP_LOADA, 8, src_element_ptr, temporary_result_register, _01);

                builder.build_instruction(OP_ADDR_MEMCPY, element_byte_count, dest_element_ptr, src_element_ptr, _11);
            }else {
                builder.build_instruction(OP_STORE, visitor_result_entry->datatype->size, offset, temporary_result_register,
                                          offset_mode + _01);
            }
            break;
        }
    }
}

pair<int64_t, bool> VisitorResult::materialize_register(SageCompiler &compiler) {
    auto &builder = compiler.builder;
    auto *entry = compiler.symbol_table.lookup_by_index(symbol_table_index);
    auto opcode = OP_LOAD;

    switch (state) {
        case VisitorResultState::IMMEDIATE: {
            if (result_type->identify() == FLOAT) {
                int output_register = compiler.get_volatile_register();
                builder.build_fmove_immediate(output_register, entry->data);
                return make_pair<int, bool>(std::move(output_register), false);
            }
            return make_pair<int64_t, bool>(immediate_value, true);
        }
        case VisitorResultState::SPILLED: {
            int output_register = compiler.get_volatile_register();
            builder.build_instruction(opcode, output_register, entry->stack_offset, _00);
            return make_pair<int, bool>(std::move(output_register), false);
        }
        case VisitorResultState::REGISTER: {
            return make_pair<int, bool>(std::move(entry->assigned_register), false);
        }
        case VisitorResultState::VALUE: {
            int output_register = compiler.get_volatile_register();
            int static_pointer = compiler.get_literal_static_pointer(symbol_table_index);
            if (entry->datatype->is_array() || entry->datatype->is_pointer() || entry->datatype->is_struct()) {
                return make_pair<int, bool>(std::move(static_pointer), true);
            }
            builder.build_instruction(opcode, output_register, static_pointer, _00);
            return make_pair<int, bool>(std::move(output_register), false);
        }
        case VisitorResultState::TEMP_REGISTER: {
            return make_pair<int, bool>(std::move(temporary_result_register), false);
        }
        default:
            return make_pair<int64_t, bool>(std::move(immediate_value), true);
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
