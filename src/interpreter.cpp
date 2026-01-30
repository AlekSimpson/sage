#include <sys/syscall.h>  // for SYS_* constants
#include <unistd.h>

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

void SageInterpreter::push_stack_scope(int func_id) {
    size_t return_address = unpack_int(registers[STACK_POINTER]) - 1;
    if (return_address >= heap_pointer) {
        ErrorLogger::get().log_error_safe(
            "interpreter.cpp",
            current_linenum,
            "stackoverflow", GENERAL);
        return;
    }

    // make new current stack frame
    registers[STACK_POINTER] = int_reg_inc(registers[STACK_POINTER], -1);
    int start_stack_address = unpack_int(registers[STACK_POINTER]);
    map<int, int> cached_registers = frame_pointer->saved_caller_values;
    frame_pointer = new StackFrame(frame_pointer,
                                   cached_registers,
                                   return_address,
                                   start_stack_address,
                                   proc_line_locations[func_id]);
}

void SageInterpreter::pop_stack_scope() {
    registers[STACK_POINTER] = pack_int(frame_pointer->stack_pointer + 1);
    StackFrame *previous = frame_pointer->previous_frame;
    delete frame_pointer;
    frame_pointer = previous;
}

SageValue SageInterpreter::get_return_value() const {
    return register_to_value(registers[6]);
}

inline void SageInterpreter::load_program(bytecode _program) {
    program = _program;
}

inline void SageInterpreter::execute_add(vector<int> &operands, AddressMode &mode) {
    // _xx | OP_ADD reg, op, op
    int operand1 = mode[0] == 1 ? registers[operands[1]] : operands[1];
    int operand2 = mode[1] == 1 ? registers[operands[2]] : operands[2];
    registers[operands[0]] = operand1 + operand2;
}

inline void SageInterpreter::execute_sub(vector<int> &operands, AddressMode &mode) {
    // _xx | OP_SUB reg, op, op
    int operand1 = mode[0] == 1 ? registers[operands[1]] : operands[1];
    int operand2 = mode[1] == 1 ? registers[operands[2]] : operands[2];
    registers[operands[0]] = operand1 - operand2;
}

inline void SageInterpreter::execute_mul(vector<int> &operands, AddressMode &mode) {
    // _xx | OP_MUL reg, op, op

    int operand1 = mode[0] == 1 ? registers[operands[1]] : registers[operands[1]];
    int operand2 = mode[1] == 1 ? registers[operands[2]] : registers[operands[2]];

    registers[operands[0]] = operand1 * operand2;
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

    int operand1 = mode[0] == 1 ? registers[operands[1]] : registers[operands[1]];
    int operand2 = mode[1] == 1 ? registers[operands[2]] : registers[operands[2]];
    registers[operands[0]] = operand1 / operand2;
}

inline void SageInterpreter::execute_load(vector<int> &operands) {
    // _00 | load reg, ($fp - offset)
    int load_address = frame_pointer->stack_pointer - operands[1];
    registers[operands[0]] = memory[load_address];
}

inline void SageInterpreter::execute_store(vector<int> &operands, AddressMode &mode) {
    // _0x | store ($fp - offset), op
    int offset = operands[0];
    int store_address = frame_pointer->stack_pointer - offset;
    memory[store_address] = operands[0];
}

inline void SageInterpreter::execute_mov(vector<int> &operands, AddressMode &mode) {
    // _0x | mov reg, op
    registers[operands[0]] = operands[1];
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
    registers[21] = operands[0] == operands[1];
}

inline void SageInterpreter::execute_less_than_comparison(vector<int> &operands, AddressMode &mode) {
    // _xx | OP_LT op, op
    registers[21] = operands[0] < operands[1];
}

inline void SageInterpreter::execute_greater_than_comparison(vector<int> &operands, AddressMode &mode) {
    // _xx | gt op, op
    registers[21] = operands[0] > operands[1];
}

inline void SageInterpreter::execute_and(vector<int> &operands, AddressMode &mode) {
    // _xx | OP_AND op, op
    registers[21] = operands[0] == 1 && operands[1] == 1;
}

inline void SageInterpreter::execute_or(vector<int> &operands, AddressMode &mode) {
    // _xx | OP_OR op, op
    registers[21] = operands[0] == 1 || operands[1] == 1;
}

inline void SageInterpreter::execute_not(vector<int> &operands) {
    // _00 | OP_NOT reg
    registers[21] = !registers[operands[0]];
}

inline void SageInterpreter::execute_system_call() {
    int callcode = unpack_int(registers[22]);

    switch (callcode) {
        case SYS_write: {
            // todo: so this memory right now is leaking without the string pool, we'll need a VM cleanup function eventually
            void *complex_value = unpack_pointer(registers[1]); // Clear type tag bits
            const char *text = static_cast<const char *>(complex_value);
            syscall(
                callcode,
                unpack_int(registers[0]),
                text,
                unpack_int(registers[2]));
            break;
        }
        case SAGESYS_write_int: {
            string text = to_string(unpack_int(registers[1]));
            syscall(
                SYS_write,
                unpack_int(registers[0]),
                text.c_str(),
                unpack_int(registers[2]));
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
            case OP_LOAD:
                execute_load(operands);
                break;
            case OP_STORE:
                execute_store(operands, current_command.address_mode);
                break;
            case OP_MOV:
                execute_mov(operands, current_command.address_mode);
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
        if (symbol_entry->type->identify() != ARRAY) {
            ErrorLogger::get().log_internal_error_safe("interpreter.cpp", current_linenum, "Unsupported static member found.");
            continue;
        }

        for (int i = 0; i < symbol_memory_chunk.size(); ++i) {
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
