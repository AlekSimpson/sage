#include "../include/platform.h"
#include "../include/interpreter.h"

#include <bytecode_builder.h>
#include <codegen.h>

#include "../include/sage_types.h"
#include "../include/registers.h"
#include "../include/error_logger.h"


// StackFrame

StackFrame::StackFrame(
    StackFrame *previous, map<int, int> caller_cache, int ret_addr, size_t sp, int program_start
) : prog_return_address(ret_addr), prog_start_address(program_start), stack_pointer(sp), previous_frame(previous),
    saved_caller_values(caller_cache) {
}

StackFrame::StackFrame()
    : prog_return_address(-1), prog_start_address(0), stack_pointer(0), previous_frame(nullptr) {
}

// Interpreter

SageInterpreter::SageInterpreter() : frame_pointer(nullptr) {}

SageInterpreter::SageInterpreter(SageSymbolTable *table)
: symbol_table(table), frame_pointer(nullptr) {
}

uint8_t *SageInterpreter::memory_read_bytes(uint8_t address) {
    return &memory[address];
}

void SageInterpreter::stack_write_i32(size_t addr, int32_t val) {
    memory[addr + 0] = (val >> 0)  & 0xFF;
    memory[addr + 1] = (val >> 8)  & 0xFF;
    memory[addr + 2] = (val >> 16) & 0xFF;
    memory[addr + 3] = (val >> 24) & 0xFF;
}

int32_t SageInterpreter::stack_read_i32(size_t addr) {
    return memory[addr - 0]    |
       (memory[addr - 1] << 8) |
       (memory[addr - 2] << 16)|
       (memory[addr - 3] << 24);
}

size_t SageInterpreter::allocate_on_heap(size_t bytes) {
    size_t start_address = heap_pointer;
    heap_pointer += bytes;
    return start_address;
}

size_t SageInterpreter::allocate_on_stack(size_t bytes) {
    size_t start_address = stack_pointer();
    set_register(STACK_POINTER, start_address - bytes);
    return start_address;
}

void SageInterpreter::push_stack_scope(int func_id) {
    // make new current stack frame
    int program_return_address = program_pointer + 1;
    set_register(STACK_POINTER, stack_pointer() - 1);
    int start_stack_address = stack_pointer();
    map<int, int> cached_registers = frame_pointer->saved_caller_values;
    frame_pointer = new StackFrame(frame_pointer,
                                   cached_registers,
                                   program_return_address,
                                   start_stack_address,
                                   proc_line_locations[func_id]);
}

void SageInterpreter::pop_stack_scope() {
    set_register(STACK_POINTER, frame_pointer->stack_pointer + 1);
    StackFrame *previous = frame_pointer->previous_frame;
    delete frame_pointer;
    frame_pointer = previous;
}

inline int SageInterpreter::stack_pointer() {
    return unpack_int(registers[STACK_POINTER]);
}

int SageInterpreter::read_register(int _register) {
    return unpack_int(registers[_register]);
}

SageValue SageInterpreter::get_return_value() const {
    // TODO: float return? other return types? function / type returns?
    return unpack_int(registers[6]);
}

void SageInterpreter::load_program(bytecode _program) {
    program = _program;
}

void SageInterpreter::set_register(int _register, int value) {
    registers[_register] = pack_int(value);
}

void SageInterpreter::set_float_register(int _register, double value) {
    floating_point_registers[_register] = value;
}

inline void SageInterpreter::execute_add(vector<int> &operands, AddressMode &mode) {
    // _xx | OP_ADD reg, op, op
    int operand1 = mode[0] == 1 ? read_register(operands[1]) : operands[1];
    int operand2 = mode[1] == 1 ? read_register(operands[2]) : operands[2];
    set_register(operands[0], operand1 + operand2);
}

inline void SageInterpreter::execute_float_add(vector<int> &operands, AddressMode &mode) {
    // _xx | OP_FADD reg, op, op
    int operand1 = mode[0] == 1 ? floating_point_registers[operands[1]] : operands[1];
    int operand2 = mode[1] == 1 ? floating_point_registers[operands[2]] : operands[2];
    floating_point_registers[operands[0]] = operand1 + operand2;
}

inline void SageInterpreter::execute_sub(vector<int> &operands, AddressMode &mode) {
    // _xx | OP_SUB reg, op, op
    int operand1 = mode[0] == 1 ? read_register(operands[1]) : operands[1];
    int operand2 = mode[1] == 1 ? read_register(operands[2]) : operands[2];
    set_register(operands[0], operand1 - operand2);
}

inline void SageInterpreter::execute_float_sub(vector<int> &operands, AddressMode &mode) {
    // _xx | OP_FSUB reg, op, op
    int operand1 = mode[0] == 1 ? floating_point_registers[operands[1]] : operands[1];
    int operand2 = mode[1] == 1 ? floating_point_registers[operands[2]] : operands[2];
    floating_point_registers[operands[0]] = operand1 - operand2;
}

inline void SageInterpreter::execute_mul(vector<int> &operands, AddressMode &mode) {
    // _xx | OP_MUL reg, op, op

    int operand1 = mode[0] == 1 ? read_register(operands[1]) : operands[1];
    int operand2 = mode[1] == 1 ? read_register(operands[2]) : operands[2];

    set_register(operands[0], operand1 * operand2);
}

inline void SageInterpreter::execute_float_mul(vector<int> &operands, AddressMode &mode) {
    // _xx | OP_FMUL reg, op, op

    int operand1 = mode[0] == 1 ? floating_point_registers[operands[1]] : operands[1];
    int operand2 = mode[1] == 1 ? floating_point_registers[operands[2]] : operands[2];

    floating_point_registers[operands[0]] = operand1 * operand2;
}

inline void SageInterpreter::execute_div(vector<int> &operands, AddressMode &mode) {
    // _xx | OP_DIV reg, op, op
    if (operands[2] == 0) {
        ErrorLogger::get().log_internal_error_safe(
            "interpreter.cpp",
            current_linenum,
            "Division by zero.");
        return;
    }

    int operand1 = mode[0] == 1 ? read_register(operands[1]) : operands[1];
    int operand2 = mode[1] == 1 ? read_register(operands[2]) : operands[2];
    set_register(operands[0], operand1 / operand2);
}

inline void SageInterpreter::execute_float_div(vector<int> &operands, AddressMode &mode) {
    // _xx | OP_FDIV reg, op, op
    if (operands[2] == 0) {
        ErrorLogger::get().log_internal_error_safe(
            "interpreter.cpp",
            current_linenum,
            "Division by zero.");
        return;
    }

    int operand1 = mode[0] == 1 ? floating_point_registers[operands[1]] : operands[1];
    int operand2 = mode[1] == 1 ? floating_point_registers[operands[2]] : operands[2];
    floating_point_registers[operands[0]] = operand1 / operand2;
}

inline void SageInterpreter::execute_load(vector<int> &operands) {
    // _00 | load reg, ($fp - offset)
    int load_address = frame_pointer->stack_pointer - operands[1];
    set_register(operands[0], memory[load_address]);
}

inline void SageInterpreter::execute_float_load(vector<int> &operands) {
    // _00 | fload reg, ($fp - offset)
    int load_address = frame_pointer->stack_pointer - operands[1];
    floating_point_registers[operands[0]] = memory[load_address];
}

inline void SageInterpreter::execute_store(vector<int> &operands, AddressMode &mode) {
    // _0x | store ($fp - offset), op
    int offset = mode[0] == 1 ? read_register(operands[0]) : operands[0];
    int store_address = frame_pointer->stack_pointer - offset;
    int operand1 = mode[1] == 1 ? floating_point_registers[operands[1]] : operands[1];
    memory[store_address] = operand1;
}

inline void SageInterpreter::execute_float_store(vector<int> &operands, AddressMode &mode) {
    // _0x | fstore ($fp - offset), op
    int offset = operands[0];
    int store_address = frame_pointer->stack_pointer - offset;
    int operand1 = mode[1] == 1 ? floating_point_registers[operands[1]] : operands[1];
    memory[store_address] = operand1;
}

void SageInterpreter::execute_stack_allocate(int bytes) {
    // _00 | alloc size    | size in bytes
    set_register(STACK_POINTER, stack_pointer() - bytes);
}

inline void SageInterpreter::execute_move(vector<int> &operands, AddressMode &mode) {
    // _0x | mov reg, op
    set_register(operands[0], mode[1] == 1 ? read_register(operands[1]) : operands[1]);
}

inline void SageInterpreter::execute_float_move(vector<int> &operands, AddressMode &mode) {
    // _0x | fmov reg, op
    set_float_register(operands[0], mode[1] == 1 ? floating_point_registers[operands[1]] : operands[1]);
}

inline void SageInterpreter::execute_call(vector<int> &operands) {
    // _00 | call immediate
    int caller_dest_location = operands[0];
    push_stack_scope(caller_dest_location);
    program_pointer = frame_pointer->prog_start_address;
}

inline void SageInterpreter::execute_return() {
    int callback_addr = frame_pointer->prog_return_address;
    if (callback_addr == -1) {
        vm_running = false;
    }
    program_pointer = callback_addr;
    pop_stack_scope();
}

inline void SageInterpreter::execute_equality_comparison(vector<int> &operands, AddressMode &mode) {
    // _xx | OP_EQ op, op
    int operand0 = mode[0] == 1 ? read_register(operands[0]) : operands[0];
    int operand1 = mode[1] == 1 ? read_register(operands[1]) : operands[1];
    set_register(21, operand0 == operand1);
}

inline void SageInterpreter::execute_less_than_comparison(vector<int> &operands, AddressMode &mode) {
    // _xx | OP_LT op, op
    int operand0 = mode[0] == 1 ? read_register(operands[0]) : operands[0];
    int operand1 = mode[1] == 1 ? read_register(operands[1]) : operands[1];
    set_register(21, operand0 < operand1);
}

inline void SageInterpreter::execute_greater_than_comparison(vector<int> &operands, AddressMode &mode) {
    // _xx | gt op, op
    int operand0 = mode[0] == 1 ? read_register(operands[0]) : operands[0];
    int operand1 = mode[1] == 1 ? read_register(operands[1]) : operands[1];
    set_register(21, operand0 > operand1);
}

inline void SageInterpreter::execute_and(vector<int> &operands, AddressMode &mode) {
    // _xx | OP_AND op, op
    int operand0 = mode[0] == 1 ? read_register(operands[0]) : operands[0];
    int operand1 = mode[1] == 1 ? read_register(operands[1]) : operands[1];
    set_register(21, operand0 == 1 && operand1 == 0);
}

inline void SageInterpreter::execute_or(vector<int> &operands, AddressMode &mode) {
    // _xx | OP_OR op, op
    int operand0 = mode[0] == 1 ? read_register(operands[0]) : operands[0];
    int operand1 = mode[1] == 1 ? read_register(operands[1]) : operands[1];
    set_register(21, operand0 == 1 || operand1 == 1);
}

inline void SageInterpreter::execute_not(vector<int> &operands) {
    // _00 | OP_NOT reg
    set_register(21, !read_register(operands[0]));
}

inline void SageInterpreter::execute_float_equality_comparison(vector<int> &operands, AddressMode &mode) {
    int operand1 = mode[0] == 1 ? floating_point_registers[operands[0]] : operands[0];
    int operand2 = mode[1] == 1 ? floating_point_registers[operands[1]] : operands[1];
    set_register(21, operand1 == operand2);
}

inline void SageInterpreter::execute_float_less_than_comparison(vector<int> &operands, AddressMode &mode) {
    int operand1 = mode[0] == 1 ? floating_point_registers[operands[0]] : operands[0];
    int operand2 = mode[1] == 1 ? floating_point_registers[operands[1]] : operands[1];
    set_register(21, operand1 < operand2);
}

inline void SageInterpreter::execute_float_greater_than_comparison(vector<int> &operands, AddressMode &mode) {
    int operand1 = mode[0] == 1 ? floating_point_registers[operands[0]] : operands[0];
    int operand2 = mode[1] == 1 ? floating_point_registers[operands[1]] : operands[1];
    set_register(21, operand1 > operand2);
}

inline void SageInterpreter::execute_system_call() {
    SVM_SYSCALL callcode = (SVM_SYSCALL)read_register(22);

    switch (callcode) {
        case SYS_WRITE: {
            int static_pointer = read_register(1);
            int character_count = read_register(2);
            string characters;
            for (int i = 0; i < character_count; i++) {
                characters += (char)memory[static_pointer];
                static_pointer++;
            }
            const char *buffer = characters.c_str();
            sage_write(
                read_register(0),
                buffer,
                character_count);
            break;
        }
        case SYS_WRITE_INT: {
            string text = to_string(read_register(1));
            sage_write(
                read_register(0),
                text.c_str(),
                read_register(2));
            break;
        }
        default:
            break;
    }
}

void SageInterpreter::execute() {
    if (frame_pointer == nullptr) return;

    program_pointer = proc_line_locations[get_procedure_frame_id("GLOBAL")];
    registers[STACK_POINTER] = 0;

    vector<int> operands;
    Command current_command;
    vm_running = true;
    bool prog_pointer_jump = false;

    while (vm_running && program_pointer < (int) program.size()) {
        if (ErrorLogger::get().has_errors()) {
            ErrorLogger::get().report_errors();
            break;
        }
        current_command = program[program_pointer];
        auto debug_string = current_command.print();

         operands = current_command.instruction.unpack_instruction();
        switch (current_command.instruction.opcode) {
            case OP_FADD:
                execute_float_add(operands, current_command.address_mode);
                break;
            case OP_FSUB:
                execute_float_sub(operands, current_command.address_mode);
                break;
            case OP_FMUL:
                execute_float_mul(operands, current_command.address_mode);
                break;
            case OP_FDIV:
                execute_float_div(operands, current_command.address_mode);
                break;
            case OP_ADD:
                execute_add(operands, current_command.address_mode);
                break;
            case OP_SUB:
                execute_sub(operands, current_command.address_mode);
                break;
            case OP_MUL:
                execute_mul(operands, current_command.address_mode);
                break;
            case OP_DIV:
                execute_div(operands, current_command.address_mode);
                break;
            case OP_ALLOC:
                execute_stack_allocate(operands[0]);
                break;
            case OP_LOAD:
                execute_load(operands);
                break;
            case OP_FLOAD:
                execute_float_load(operands);
                break;
            case OP_STORE:
                execute_store(operands, current_command.address_mode);
                break;
            case OP_FSTORE:
                execute_float_store(operands, current_command.address_mode);
                break;
            case OP_MOV:
                execute_move(operands, current_command.address_mode);
                break;
            case OP_FMOV:
                execute_float_move(operands, current_command.address_mode);
                break;
            case OP_JMP:
                break; // TODO:
            case OP_JZ:
                break; // TODO:
            case OP_JNZ:
                break; // TODO:
            case OP_CALL:
                execute_call(operands);
                prog_pointer_jump = true;
                break;
            case OP_RET:
                execute_return();
                prog_pointer_jump = true;
                break;
            case OP_EQ:
                execute_equality_comparison(operands, current_command.address_mode);
                break;
            case OP_LT:
                execute_less_than_comparison(operands, current_command.address_mode);
                break;
            case OP_GT:
                execute_greater_than_comparison(operands, current_command.address_mode);
                break;
            case OP_AND:
                execute_and(operands, current_command.address_mode);
                break;
            case OP_OR:
                execute_or(operands, current_command.address_mode);
                break;
            case OP_NOT:
                execute_not(operands);
                break;
            case OP_FEQ:
                execute_float_equality_comparison(operands, current_command.address_mode);
                break;
            case OP_FLT:
                execute_float_less_than_comparison(operands, current_command.address_mode);
                break;
            case OP_FGT:
                execute_float_greater_than_comparison(operands, current_command.address_mode);
                break;
            case OP_SYSCALL:
                execute_system_call();
                break;
            case OP_LABEL:
            case OP_NOP:
                break;
            case VOP_EXIT:
                vm_running = false;
                continue;
            default:
                ErrorLogger::get().log_internal_error_safe(
                    "interpreter.cpp",
                    current_linenum,
                    "VM tried to execute unrecognized bytecode");
                vm_running = false;
                continue;
        }

        if (!prog_pointer_jump) {
            program_pointer++;
        }
        prog_pointer_jump = false;
    }
}

void SageInterpreter::open(const map<int, int> &procedure_line_locations, map<table_index, vector<uint8_t>> &static_section_components) {
    if (frame_pointer == nullptr) frame_pointer = new StackFrame();

    const int megabyte = 1024 * 1024;
    memory.resize(megabyte);

    size_t static_working_pointer = static_start_pointer;
    for (const auto &[static_symbol_index, symbol_memory_chunk]: static_section_components) {
        auto *symbol_entry = symbol_table->lookup_by_index(static_symbol_index);
        if (symbol_entry->type->identify() != ARRAY && symbol_entry->type->identify() != FLOAT) {
            ErrorLogger::get().log_internal_error_safe("interpreter.cpp", current_linenum, "Unsupported static member found.");
            continue;
        }

        symbol_entry->static_stack_pointer = static_working_pointer;
        for (int i = 0; i < (int)symbol_memory_chunk.size(); ++i) {
            memory[static_working_pointer] = symbol_memory_chunk[i];
            static_working_pointer++;
        }
    }
    heap_pointer = static_working_pointer;
    static_memory_end_pointer = static_working_pointer - 1;
    registers[23] = memory.size()-1; // stack begins are memory max and "grows up"

    proc_line_locations = procedure_line_locations;
    vm_running = false;
}

void SageInterpreter::close() {
    memory.clear();
    program.clear();
    // deinit stackframe when done with interpreter
    if (frame_pointer != nullptr) {
        if (frame_pointer->previous_frame == nullptr) {
            delete frame_pointer;
            return;
        }

        StackFrame *above = nullptr;
        StackFrame *walk = frame_pointer;
        while (walk->previous_frame != nullptr) {
            above = walk;
            walk = walk->previous_frame;
            delete above;
        }
    }
}
