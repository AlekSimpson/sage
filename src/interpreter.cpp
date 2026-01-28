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
    int32_t return_address = program_pointer + 1;
    if (return_address + 1 == (int) stack.capacity()) {
        ErrorLogger::get().log_error_safe(
            "interpreter.cpp",
            current_linenum,
            "stackoverflow", GENERAL);
        return;
    }

    // make new current stack frame
    registers[STACK_POINTER] = int_reg_inc(registers[STACK_POINTER], 1);
    int start_stack_address = unpack_int(registers[STACK_POINTER]);
    map<int, int> cached_registers = frame_pointer->saved_caller_values;
    frame_pointer = new StackFrame(frame_pointer,
                                   cached_registers,
                                   return_address,
                                   start_stack_address,
                                   proc_line_locations[func_id]);
}

void SageInterpreter::pop_stack_scope() {
    registers[STACK_POINTER] = pack_int(frame_pointer->stack_pointer - 1);
    StackFrame *previous = frame_pointer->previous_frame;
    delete frame_pointer;
    frame_pointer = previous;
}

SageValue SageInterpreter::get_return_value() const {
    return register_to_value(registers[6]);
}

void SageInterpreter::load_program(bytecode _program) {
    program = _program;
}

vector<SageValue> SageInterpreter::dereference_map(instruction *inst, int map[4]) {
    vector<int> raw_operands = inst->read();
    vector<SageValue> return_values;
    return_values.reserve(4);

    if (inst->opcode == OP_LABEL || inst->opcode == OP_NOP || inst->opcode == VOP_EXIT) {
        return return_values;
    }

    for (int i = 0; i < (int) raw_operands.size(); ++i) {
        switch (map[i]) {
            case 0:
                // Raw immediate value
                return_values.push_back(SageValue(raw_operands[i], TypeRegistery::get_integer_type(4)));
                break;
            case 1:
                // Dereference register
                return_values.push_back(SageValue(registers[raw_operands[i]]));
                break;
            default:
                break;
        }
    }

    return return_values;
}

void SageInterpreter::execute_add(vector<SageValue> operands) {
    if (operands.size() < 3) {
        ErrorLogger::get().log_internal_error_safe(
            "interpreter.cpp",
            current_linenum,
            "execution requires 3 operands but less were found!");
        return;
    }

    int target_register = operands[0].as_operand();

    bool first_operator_is_float = (operands[1].valuetype->identify() == FLOAT);
    bool second_operator_is_float = (operands[2].valuetype->identify() == FLOAT);
    if (first_operator_is_float || second_operator_is_float) {
        registers[target_register] = SageValue(operands[1].as_float() + operands[2].as_float());
        return;
    }

    registers[target_register] = SageValue(operands[1].as_i32() + operands[2].as_i32());
}

void SageInterpreter::execute_sub(vector<SageValue> operands) {
    if (operands.size() < 3) {
        ErrorLogger::get().log_internal_error_safe(
            "interpreter.cpp",
            current_linenum,
            "execution requires 3 operands but less were found!");
        return;
    }

    int target_register = operands[0].as_operand();

    bool first_operator_is_float = (operands[1].valuetype->identify() == FLOAT);
    bool second_operator_is_float = (operands[2].valuetype->identify() == FLOAT);
    if (first_operator_is_float || second_operator_is_float) {
        registers[target_register] = SageValue(operands[1].as_float() - operands[2].as_float());
        return;
    }

    registers[target_register] = SageValue(operands[1].as_i32() - operands[2].as_i32());
}

void SageInterpreter::execute_mul(vector<SageValue> operands) {
    if (operands.size() < 3) {
        ErrorLogger::get().log_internal_error_safe(
            "interpreter.cpp",
            current_linenum,
            "execution requires 3 operands but less were found!");
        return;
    }

    int target_register = operands[0].as_operand();

    bool first_operator_is_float = (operands[1].valuetype->identify() == FLOAT);
    bool second_operator_is_float = (operands[2].valuetype->identify() == FLOAT);
    if (first_operator_is_float || second_operator_is_float) {
        registers[target_register] = SageValue(operands[1].as_float() * operands[2].as_float());
        return;
    }

    registers[target_register] = SageValue(operands[1].as_i32() * operands[2].as_i32());
}

void SageInterpreter::execute_div(vector<SageValue> operands) {
    if (operands.size() < 3) {
        ErrorLogger::get().log_internal_error_safe(
            "interpreter.cpp",
            current_linenum,
            "execution requires 3 operands but less were found!");
        return;
    }

    if (operands[2].value.int_value == 0) {
        ErrorLogger::get().log_internal_error_safe(
            "interpreter.cpp",
            current_linenum,
            "execution requires 3 operands but less were found!");
        return;
    }

    int target_register = operands[0].as_operand();

    bool first_operator_is_float = (operands[1].valuetype->identify() == FLOAT);
    bool second_operator_is_float = (operands[2].valuetype->identify() == FLOAT);
    if (first_operator_is_float || second_operator_is_float) {
        registers[target_register] = SageValue(operands[1].as_float() / operands[2].as_float());
        return;
    }

    registers[target_register] = SageValue(operands[1].as_i32() / operands[2].as_i32());
}

void SageInterpreter::execute_load(vector<SageValue> operands) {
    if (operands.size() < 2) {
        ErrorLogger::get().log_internal_error_safe(
            "interpreter.cpp",
            current_linenum,
            "execution requires 2 operands but less were found!");
        return;
    }

    int load_address = unpack_int(registers[STACK_POINTER]) - operands[1].as_i32();
    registers[unpack_int(operands[0])] = memory[load_address];
}

void SageInterpreter::execute_store(vector<SageValue> operands) {
    if (operands.size() < 2) {
        ErrorLogger::get().log_internal_error_safe(
            "interpreter.cpp",
            current_linenum,
            "execution requires 2 operands but less were found!");
        return;
    }

    int offset = operands[1].as_i32();
    int store_address = unpack_int(registers[STACK_POINTER]) - offset;
    memory[store_address] = operands[0];
}

void SageInterpreter::execute_mov(vector<SageValue> operands) {
    if (operands.size() < 2) {
        ErrorLogger::get().log_internal_error_safe(
            "interpreter.cpp",
            current_linenum,
            "execution requires 2 operands but less were found!");
        return;
    }

    int dest = operands[1].as_i32();
    registers[dest] = operands[0];
}

void SageInterpreter::execute_call(vector<SageValue> operands) {
    int caller_dest_location = operands[0].as_i32();
    push_stack_scope(caller_dest_location);
    program_pointer = frame_pointer->prog_start_address;
}

void SageInterpreter::execute_return() {
    int callback_addr = frame_pointer->prog_return_address;
    if (callback_addr == -1) {
        vm_running = false;
    }
    program_pointer = callback_addr;
    pop_stack_scope();
}

void SageInterpreter::execute_eqcomp(vector<SageValue> operands) {
    registers[21] = SageValue(operands[0].equals(operands[1]), TypeRegistery::get_byte_type(BOOL));
}

void SageInterpreter::execute_ltcomp(vector<SageValue> operands) {
    bool first_operator_is_float = (operands[0].valuetype->identify() == FLOAT);
    bool second_operator_is_float = (operands[1].valuetype->identify() == FLOAT);
    if (first_operator_is_float || second_operator_is_float) {
        registers[21] = SageValue(operands[0].as_float() < operands[1].as_float(),
                                  TypeRegistery::get_byte_type(BOOL));
    }

    registers[21] = SageValue(operands[0].as_i32() < operands[1].as_i32(), TypeRegistery::get_byte_type(BOOL));
}

void SageInterpreter::execute_gtcomp(vector<SageValue> operands) {
    bool first_operator_is_float = (operands[0].valuetype->identify() == FLOAT);
    bool second_operator_is_float = (operands[1].valuetype->identify() == FLOAT);
    if (first_operator_is_float || second_operator_is_float) {
        registers[21] = SageValue(operands[0].as_float() > operands[1].as_float(),
                                  TypeRegistery::get_byte_type(BOOL));
    }

    registers[21] = SageValue(operands[0].as_i32() > operands[1].as_i32(), TypeRegistery::get_byte_type(BOOL));
}

void SageInterpreter::execute_and(vector<SageValue> operands) {
    registers[21] = SageValue((operands[0].as_i32() && operands[1].as_i32()) == 1,
                              TypeRegistery::get_byte_type(BOOL));
}

void SageInterpreter::execute_or(vector<SageValue> operands) {
    registers[21] = SageValue((operands[0].as_i32() && operands[1].as_i32()) == 1,
                              TypeRegistery::get_byte_type(BOOL));
}

void SageInterpreter::execute_not(vector<SageValue> operands) {
    int value = unpack_int(operands[0]);
    registers[21] = pack_int(!value);
    registers[21] = SageValue((!operands[0].as_i32()),
                              TypeRegistery::get_byte_type(BOOL));
}

void SageInterpreter::execute_syscall() {
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

    command current_command;
    vector<SageValue> operands;
    vm_running = true;
    bool prog_pointer_jump = false;

    while (vm_running && program_pointer < (int) program.size()) {
        if (ErrorLogger::get().has_errors()) {
            ErrorLogger::get().report_errors();
            break;
        }
        current_command = program[program_pointer];
        auto debug_string = current_command.print();

        operands = dereference_map(&current_command.inst, current_command.deref_map);
        switch (current_command.inst.opcode) {
            case OP_ADD:
                execute_add(operands);
                break;
            case OP_SUB:
                execute_sub(operands);
                break;
            case OP_MUL:
                execute_mul(operands);
                break;
            case OP_DIV:
                execute_div(operands);
                break;
            case OP_LOAD:
                execute_load(operands);
                break;
            case OP_STORE:
                execute_store(operands);
                break;
            case OP_MOV:
                execute_mov(operands);
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
                execute_eqcomp(operands);
                break;
            case OP_LT:
                execute_ltcomp(operands);
                break;
            case OP_GT:
                execute_gtcomp(operands);
                break;
            case OP_AND:
                execute_and(operands);
                break;
            case OP_OR:
                execute_or(operands);
                break;
            case OP_NOT:
                execute_not(operands);
                break;
            case OP_SYSCALL:
                execute_syscall();
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
