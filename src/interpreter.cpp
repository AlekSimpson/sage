#include "../include/platform.h"
#include "../include/interpreter.h"

#include <bytecode_builder.h>
#include <codegen.h>
#include <cstring>
#include <sage_value.h>

#include "../include/sage_types.h"
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

size_t SageInterpreter::allocate_on_heap(size_t bytes) {
    size_t start_address = heap_pointer;
    heap_pointer += bytes;
    return start_address;
}

size_t SageInterpreter::allocate_on_stack(size_t bytes) {
    size_t start_address = stack_pointer();
    registers[STACK_POINTER] = start_address - bytes;
    return start_address;
}

void SageInterpreter::push_stack_scope(int func_id) {
    // make new current stack frame
    int program_return_address = program_pointer + 1;
    registers[STACK_POINTER] = stack_pointer() - 1;
    int start_stack_address = stack_pointer();
    map<int, int> cached_registers = frame_pointer->saved_caller_values;
    frame_pointer = new StackFrame(frame_pointer,
                                   cached_registers,
                                   program_return_address,
                                   start_stack_address,
                                   proc_line_locations[func_id]);
}

void SageInterpreter::pop_stack_scope() {
    registers[STACK_POINTER] = frame_pointer->stack_pointer + 1;
    StackFrame *previous = frame_pointer->previous_frame;
    delete frame_pointer;
    frame_pointer = previous;
}

inline int SageInterpreter::stack_pointer() {
    return registers[STACK_POINTER];
}

SageValue SageInterpreter::get_return_value() const {
    return SageValue();
}

void SageInterpreter::load_program(bytecode _program) {
    program = _program;
}

double SageInterpreter::read_float_register(int reg) {
    double return_value;
    std::memcpy(&return_value, &registers[reg], sizeof(double));
    return return_value;
}

void SageInterpreter::set_float_register(int reg, double value) {
    std::memcpy(&registers[reg], &value, sizeof(double));
}

inline void SageInterpreter::execute_add(std::array<int64_t, 3> &operands, AddressMode &mode) {
    // _xx | OP_ADD reg, op, op
    int operand1 = mode[0] == 1 ? registers[operands[1]] : operands[1];
    int operand2 = mode[1] == 1 ? registers[operands[2]] : operands[2];
    registers[operands[0]] = operand1 + operand2;
}

inline void SageInterpreter::execute_float_add(std::array<int64_t, 3> &operands) {
    // _xx | OP_FADD reg, op, op
    double operand1 = read_float_register(operands[1]);
    double operand2 = read_float_register(operands[2]);
    set_float_register(operands[0], operand1 + operand2);
}

inline void SageInterpreter::execute_sub(std::array<int64_t, 3> &operands, AddressMode &mode) {
    // _xx | OP_SUB reg, op, op
    int operand1 = mode[0] == 1 ? registers[operands[1]] : operands[1];
    int operand2 = mode[1] == 1 ? registers[operands[2]] : operands[2];
    registers[operands[0]] = operand1 - operand2;
}

inline void SageInterpreter::execute_float_sub(std::array<int64_t, 3> &operands) {
    // _xx | OP_FSUB reg, op, op
    double operand1 = read_float_register(operands[1]);
    double operand2 = read_float_register(operands[2]);
    set_float_register(operands[0], operand1 - operand2);
}

inline void SageInterpreter::execute_mul(std::array<int64_t, 3> &operands, AddressMode &mode) {
    // _xx | OP_MUL reg, op, op

    int operand1 = mode[0] == 1 ? registers[operands[1]] : operands[1];
    int operand2 = mode[1] == 1 ? registers[operands[2]] : operands[2];
    registers[operands[0]] = operand1 * operand2;
}

inline void SageInterpreter::execute_float_mul(std::array<int64_t, 3> &operands) {
    // _xx | OP_FMUL reg, op, op

    double operand1 = read_float_register(operands[1]);
    double operand2 = read_float_register(operands[2]);
    set_float_register(operands[0], operand1 * operand2);
}

inline void SageInterpreter::execute_div(std::array<int64_t, 3> &operands, AddressMode &mode) {
    // _xx | OP_DIV reg, op, op
    assertm(operands[2] != 0, "Division by zero.");

    int operand1 = mode[0] == 1 ? registers[operands[1]] : operands[1];
    int operand2 = mode[1] == 1 ? registers[operands[2]] : operands[2];
    registers[operands[0]] = operand1 / operand2;
}

inline void SageInterpreter::execute_float_div(std::array<int64_t, 3> &operands) {
    // _xx | OP_FDIV reg, op, op
    assertm(operands[2] != 0, "Division by zero.");

    double operand1 = read_float_register(operands[1]);
    double operand2 = read_float_register(operands[2]);
    set_float_register(operands[0], operand1 / operand2);
}

inline void SageInterpreter::execute_load(std::array<int64_t, 3> &operands, AddressMode &mode) {
    // _0x | load bytes, reg, ($fp - offset)
    int bytes = operands[0];
    int offset = mode[1] == 1 ? registers[operands[2]] : operands[2];
    int load_address = frame_pointer->stack_pointer - offset;
    std::memcpy(&registers[operands[1]], &memory[load_address], bytes);
}

inline void SageInterpreter::execute_store(std::array<int64_t, 3> &operands, AddressMode &mode) {
    // _xx | store bytes, ($fp - offset), op
    int bytes = operands[0];

    int dest_offset = mode[0] == 1 ? registers[operands[1]] : operands[1];
    int dest_address = frame_pointer->stack_pointer - dest_offset;
    int64_t payload_value = mode[1] == 1 ? registers[operands[2]] : operands[2];

    std::memcpy(&memory[dest_address], &payload_value, bytes);
}

void SageInterpreter::execute_stack_allocate(int bytes) {
    // _00 | alloc size    | size in bytes
    registers[STACK_POINTER] = stack_pointer() - bytes;
}

inline void SageInterpreter::execute_move(std::array<int64_t, 3> &operands, AddressMode &mode) {
    // _0x | mov reg, op
    registers[operands[0]] = mode[1] == 1 ? registers[operands[1]] : operands[1];
}

inline void SageInterpreter::execute_int_to_float_move(std::array<int64_t, 3> &operands) {
    // _01 | itfmov freg, ireg
    set_float_register(operands[0], (double)registers[operands[1]]);
}

inline void SageInterpreter::execute_float_move(std::array<int64_t, 3> &operands, AddressMode &mode) {
    // _0x | fmov reg, op
    double default_value;
    std::memcpy(&default_value, &operands[1], sizeof(double));
    double value = mode[1] == 1 ? read_float_register(operands[1]) : default_value;
    set_float_register(operands[0], value);
}

inline void SageInterpreter::execute_call(std::array<int64_t, 3> &operands) {
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

inline void SageInterpreter::execute_equality_comparison(std::array<int64_t, 3> &operands, AddressMode &mode) {
    // _xx | OP_EQ op, op
    int operand0 = mode[0] == 1 ? registers[operands[0]] : operands[0];
    int operand1 = mode[1] == 1 ? registers[operands[1]] : operands[1];
    registers[21] = operand0 == operand1;
}

inline void SageInterpreter::execute_less_than_comparison(std::array<int64_t, 3> &operands, AddressMode &mode) {
    // _xx | OP_LT op, op
    int operand0 = mode[0] == 1 ? registers[operands[0]] : operands[0];
    int operand1 = mode[1] == 1 ? registers[operands[1]] : operands[1];
    registers[21] = operand0 < operand1;
}

inline void SageInterpreter::execute_greater_than_comparison(std::array<int64_t, 3> &operands, AddressMode &mode) {
    // _xx | gt op, op
    int operand0 = mode[0] == 1 ? registers[operands[0]] : operands[0];
    int operand1 = mode[1] == 1 ? registers[operands[1]] : operands[1];
    registers[21] = operand0 > operand1;
}

inline void SageInterpreter::execute_and(std::array<int64_t, 3> &operands, AddressMode &mode) {
    // _xx | OP_AND op, op
    int operand0 = mode[0] == 1 ? registers[operands[0]] : operands[0];
    int operand1 = mode[1] == 1 ? registers[operands[1]] : operands[1];
    registers[21] = operand0 == 1 && operand1 == 0;
}

inline void SageInterpreter::execute_or(std::array<int64_t, 3> &operands, AddressMode &mode) {
    // _xx | OP_OR op, op
    int operand0 = mode[0] == 1 ? registers[operands[0]] : operands[0];
    int operand1 = mode[1] == 1 ? registers[operands[1]] : operands[1];
    registers[21] = operand0 == 1 || operand1 == 1;
}

inline void SageInterpreter::execute_not(std::array<int64_t, 3> &operands) {
    // _00 | OP_NOT reg
    registers[21] = !operands[0];
}

inline void SageInterpreter::execute_float_equality_comparison(std::array<int64_t, 3> &operands, AddressMode &mode) {
    int operand1 = mode[0] == 1 ? read_float_register(operands[0]) : operands[0];
    int operand2 = mode[1] == 1 ? read_float_register(operands[1]) : operands[1];
    registers[21] = operand1 == operand2;
}

inline void SageInterpreter::execute_float_less_than_comparison(std::array<int64_t, 3> &operands, AddressMode &mode) {
    int operand1 = mode[0] == 1 ? read_float_register(operands[0]) : operands[0];
    int operand2 = mode[1] == 1 ? read_float_register(operands[1]) : operands[1];
    registers[21] = operand1 < operand2;
}

inline void SageInterpreter::execute_float_greater_than_comparison(std::array<int64_t, 3> &operands, AddressMode &mode) {
    int operand1 = mode[0] == 1 ? read_float_register(operands[0]) : operands[0];
    int operand2 = mode[1] == 1 ? read_float_register(operands[1]) : operands[1];
    registers[21] = operand1 > operand2;
}

void SageInterpreter::execute_mem_copy(std::array<int64_t, 3> &operands, AddressMode &mode) {
    // _xx | memcpy bytes, ($fp - dest_offset), ($fp - src_offset)
    int bytes = operands[0];
    int dest_offset = mode[0] == 1 ? registers[operands[1]] : operands[1];
    int dest_address = frame_pointer->stack_pointer - dest_offset;

    int src_offset = mode[1] == 1 ? registers[operands[2]] : operands[2];
    int src_address = frame_pointer->stack_pointer - src_offset;

    std::memcpy(&memory[dest_address], &memory[src_address], bytes);
}

void SageInterpreter::execute_static_copy(std::array<int64_t, 3> &operands, AddressMode &mode) {
    // _xx | statcpy bytes, ($fp - dest_offset), $pointer
    int bytes = operands[0];
    int dest_offset = mode[0] == 1 ? registers[operands[1]] : operands[1];
    int dest_address = frame_pointer->stack_pointer - dest_offset;

    int src_address = mode[1] == 1 ? registers[operands[2]] : operands[2];

    std::memcpy(&memory[dest_address], &memory[src_address], bytes);
}

void SageInterpreter::execute_load_reference(std::array<int64_t, 3> &operands, AddressMode &mode) {
    int dest_register = operands[0];
    int offset = mode[1] == 1 ? registers[operands[1]] : operands[1];
    int reference_pointer = frame_pointer->stack_pointer - offset;
    registers[dest_register] = reference_pointer;
}

void SageInterpreter::execute_load_pointer(std::array<int64_t, 3> &operands, AddressMode &mode) {
    // loadp bytes, reg, (pointer)
    int64_t bytes = operands[0];
    int64_t dest_register = operands[1];
    int64_t offset = mode[1] == 1 ? registers[operands[2]] : operands[2];
    int64_t pointer = frame_pointer->stack_pointer - offset;
    int64_t deref_pointer;
    std::memcpy(&deref_pointer, &memory[pointer], bytes);
    std::memcpy(&registers[dest_register], &memory[deref_pointer], bytes);
}

inline void SageInterpreter::execute_system_call() {
    SVM_SYSCALL callcode = (SVM_SYSCALL)registers[22];

    switch (callcode) {
        case SYS_WRITE: {
            int static_pointer = registers[1];
            int character_count = registers[2];
            string characters;
            for (int i = 0; i < character_count; ++i) {
                characters += (char)memory[static_pointer + i];
            }
            const char *buffer = characters.c_str();
            sage_write(
                registers[0],
                buffer,
                character_count);
            break;
        }
        case SYS_WRITE_INT: {
            string text = to_string(registers[1]);
            sage_write(
                registers[0],
                text.c_str(),
                registers[2]);
            break;
        }
        default:
            break;
    }
}

void SageInterpreter::print_static_memory() {
    printf("STATIC_MEM[%ld]: ---------------------------------\n", ((int64_t)static_memory_end_pointer+1));
    for (int i = 0; i <= static_memory_end_pointer; ++i) {
        printf("%c", (char)memory[i]);
    }
    printf("END\n");
    printf("---------------------------------------------\n\n\n");
}

void SageInterpreter::print_stack_memory() {
    printf("STACK_MEM[%ld]: ---------------------------------\n", ((int64_t)memory.size() - stack_pointer()));
    for (int64_t i = memory.size()-1; i >= stack_pointer(); --i) {
        printf("0x%02x ", memory[i]);
    }
    printf("END\n");
    printf("---------------------------------------------\n\n\n");
}

void SageInterpreter::execute() {
    if (frame_pointer == nullptr) return;

    program_pointer = proc_line_locations[get_procedure_frame_id(GLOBAL_NAME)];

    Command current_command;
    vm_running = true;
    bool prog_pointer_jump = false;

    //print_static_memory();

    while (vm_running && program_pointer < (int) program.size()) {
        //printf("interpretting...\n");
        if (ErrorLogger::get().has_errors()) {
            ErrorLogger::get().report_errors();
            break;
        }
        current_command = program[program_pointer];
        auto debug_string = current_command.print();

        auto &operands = current_command.instruction.operands;
        switch (current_command.instruction.opcode) {
            case OP_FADD:
                execute_float_add(operands);
                break;
            case OP_FSUB:
                execute_float_sub(operands);
                break;
            case OP_FMUL:
                execute_float_mul(operands);
                break;
            case OP_FDIV:
                execute_float_div(operands);
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
                execute_load(operands, current_command.address_mode);
                break;
            case OP_LOADP:
                execute_load_pointer(operands, current_command.address_mode);
                break;
            case OP_LOADR:
                execute_load_reference(operands, current_command.address_mode);
                break;
            case OP_STORE:
                execute_store(operands, current_command.address_mode);
                break;
            case OP_MOV:
                execute_move(operands, current_command.address_mode);
                break;
            case OP_FMOV:
                execute_float_move(operands, current_command.address_mode);
                break;
            case OP_MEMCPY:
                execute_mem_copy(operands, current_command.address_mode);
                break;
            case OP_STATIC_COPY:
                execute_static_copy(operands, current_command.address_mode);
                break;
            case OP_ITF_MOV:
                execute_int_to_float_move(operands);
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
                assertm(false, "VM tried to execute unrecognized bytecode");
                continue;
        }

        if (!prog_pointer_jump) {
            program_pointer++;
        }
        prog_pointer_jump = false;
    }
}

void SageInterpreter::open(const map<int, int> &procedure_line_locations, ByteVector &program_static_memory) {
    const int megabyte = 1024 * 1024;
    memory.resize(megabyte);

    proc_line_locations = procedure_line_locations;

    if (frame_pointer == nullptr) {
        frame_pointer = new StackFrame(
            nullptr,
            procedure_line_locations,
            -1,
            memory.size()-1,
            proc_line_locations[get_procedure_frame_id(GLOBAL_NAME)]
        );
    }

    std::memcpy(memory.data(), program_static_memory.data(), program_static_memory.size());
    heap_pointer = program_static_memory.size();
    static_memory_end_pointer = program_static_memory.size() - 1;
    registers[STACK_POINTER] = memory.size()-1; // stack begins are memory max and "grows up"

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
