#include <sys/syscall.h>  // for SYS_* constants
#include <unistd.h>

#include "../include/interpreter.h"
#include "../include/sage_types.h"
#include "../include/registers.h"
#include "../include/error_logger.h"


// StackFrame

StackFrame::StackFrame(
    StackFrame* previous, map<int, int> caller_cache, int ret_addr, int sp
) : return_address(ret_addr), stack_pointer(sp), previous_frame(previous), 
    saved_caller_values(caller_cache) {}

StackFrame::StackFrame()
    : return_address(-1), stack_pointer(0), previous_frame(nullptr) {}

// Interpreter

SageInterpreter::SageInterpreter() : frame_pointer(nullptr) {}

SageInterpreter::SageInterpreter(int stack_size) {
    stack.reserve(stack_size);

    frame_pointer = new StackFrame();
}

void SageInterpreter::push_stack_scope() {
    int32_t return_address = unpack_int(registers[STACK_POINTER]); 
    if (return_address + 1 == stack.size()) {
        ErrorLogger::get().log_error(
            "interpreter.cpp",
            current_linenum,
            "stackoverflow", GENERAL);
        return;
    }

    // make new current stack frame
    registers[STACK_POINTER] = int_reg_inc(registers[STACK_POINTER], 1);
    int start_address = unpack_int(registers[STACK_POINTER]);
    map<int, int> cached_registers = frame_pointer->saved_caller_values;
    frame_pointer = new StackFrame(frame_pointer, 
		    	           cached_registers, 
				   return_address,
				   start_address);
}

void SageInterpreter::pop_stack_scope() {
    registers[STACK_POINTER] = pack_int(frame_pointer->stack_pointer-1);
    StackFrame* previous = frame_pointer->previous_frame;
    delete frame_pointer;
    frame_pointer = previous;
}

void SageInterpreter::load_program(bytecode _program) {
    program = _program;
}

vector<SageValue> SageInterpreter::dereference_map(instruction* inst, int map[4]) {
    vector<int> raw_operands = inst->read();
    vector<SageValue> return_values;
    return_values.reserve(4);

    if (inst->opcode == OP_LABEL || inst->opcode == OP_NOP || inst->opcode == VOP_EXIT) {
        return return_values;
    }

    for (int i = 0; i < raw_operands.size(); ++i) {
        if (map[i] == 0) {
            return_values.push_back(SageValue(32, raw_operands[i], TypeRegistery::get_builtin_type(I32)));
            continue;
        }

        // otherwise dereference register
        return_values.push_back(SageValue(registers[raw_operands[i]]));
    }

    return return_values;
}

void SageInterpreter::execute_add(vector<SageValue> operands) {
    if (operands.size() < 3) {
        ErrorLogger::get().log_internal_error(
            "interpreter.cpp",
            current_linenum,
            "execution requires 3 operands but less were found!");
        return;
    }

    int target_register = operands[0].as_operand();

    bool first_operator_is_float = (operands[1].valuetype->identify() == F32 || operands[1].valuetype->identify() == F64);
    bool second_operator_is_float = (operands[2].valuetype->identify() == F32 || operands[2].valuetype->identify() == F64);
    if  (first_operator_is_float || second_operator_is_float) {
        registers[target_register] = SageValue(operands[1].as_float() + operands[2].as_float());
        return;
    }

    registers[target_register] = SageValue(operands[1].as_i32() + operands[2].as_i32());
}

void SageInterpreter::execute_sub(vector<SageValue> operands) {
    if (operands.size() < 3) {
        ErrorLogger::get().log_internal_error(
           "interpreter.cpp",
           current_linenum,
           "execution requires 3 operands but less were found!");
        return;
    }

    int target_register = operands[0].as_operand();

    bool first_operator_is_float = (operands[1].valuetype->identify() == F32 || operands[1].valuetype->identify() == F64);
    bool second_operator_is_float = (operands[2].valuetype->identify() == F32 || operands[2].valuetype->identify() == F64);
    if  (first_operator_is_float || second_operator_is_float) {
        registers[target_register] = SageValue(operands[1].as_float() - operands[2].as_float());
        return;
    }

    registers[target_register] = SageValue(operands[1].as_i32() - operands[2].as_i32());
}

void SageInterpreter::execute_mul(vector<SageValue> operands) {
    if (operands.size() < 3) {
        ErrorLogger::get().log_internal_error(
          "interpreter.cpp",
          current_linenum,
          "execution requires 3 operands but less were found!");
        return;
    }

    int target_register = operands[0].as_operand();

    bool first_operator_is_float = (operands[1].valuetype->identify() == F32 || operands[1].valuetype->identify() == F64);
    bool second_operator_is_float = (operands[2].valuetype->identify() == F32 || operands[2].valuetype->identify() == F64);
    if  (first_operator_is_float || second_operator_is_float) {
        registers[target_register] = SageValue(operands[1].as_float() * operands[2].as_float());
        return;
    }

    registers[target_register] = SageValue(operands[1].as_i32() * operands[2].as_i32());
}

void SageInterpreter::execute_div(vector<SageValue> operands) {
    if (operands.size() < 3) {
        ErrorLogger::get().log_internal_error(
            "interpreter.cpp",
            current_linenum,
            "execution requires 3 operands but less were found!");
        return;
    }

    if (operands[2].value.int_value == 0) {
        ErrorLogger::get().log_internal_error(
           "interpreter.cpp",
           current_linenum,
           "execution requires 3 operands but less were found!");
        return;
    }

    int target_register = operands[0].as_operand();

    bool first_operator_is_float = (operands[1].valuetype->identify() == F32 || operands[1].valuetype->identify() == F64);
    bool second_operator_is_float = (operands[2].valuetype->identify() == F32 || operands[2].valuetype->identify() == F64);
    if  (first_operator_is_float || second_operator_is_float) {
        registers[target_register] = SageValue(operands[1].as_float() / operands[2].as_float());
        return;
    }

    registers[target_register] = SageValue(operands[1].as_i32() / operands[2].as_i32());
}

void SageInterpreter::execute_load(vector<SageValue> operands) {
    if (operands.size() < 2) {
        ErrorLogger::get().log_internal_error(
           "interpreter.cpp",
           current_linenum,
           "execution requires 2 operands but less were found!");
        return;
    }

    int load_address = unpack_int(registers[STACK_POINTER]) + operands[1].as_i32();
    registers[unpack_int(operands[0])] = stack.at(load_address);
}

void SageInterpreter::execute_store(vector<SageValue> operands) {
    if (operands.size() < 2) {
        ErrorLogger::get().log_internal_error(
            "interpreter.cpp",
            current_linenum,
            "execution requires 2 operands but less were found!");
        return;
    }

    int offset = operands[1].as_i32();
    int store_address = unpack_int(int_reg_inc(registers[STACK_POINTER], offset));
    stack[store_address] = operands[0];
}

void SageInterpreter::execute_mov(vector<SageValue> operands) {
    if (operands.size() < 2) {
        ErrorLogger::get().log_internal_error(
            "interpreter.cpp",
            current_linenum,
            "execution requires 2 operands but less were found!");
        return;
    }

    int dest = operands[1].as_i32();
    registers[dest] = operands[0];
}

void SageInterpreter::execute_call(vector<SageValue> operands) {
    // push_stack_scope(); NOTE: might actually want this to take place when we read a label instruction
    int caller_id_pointer = operands[0].as_i32();
    string* caller_id_string = heap[caller_id_pointer].value.string_value;
    program_pointer = procedure_label_encoding[*caller_id_string];
}

void SageInterpreter::execute_return() {
    int callback_addr = frame_pointer->return_address;
    program_pointer = callback_addr;
    pop_stack_scope();
}

void SageInterpreter::execute_eqcomp(vector<SageValue> operands) {
    registers[21] = SageValue(8, operands[0].equals(operands[1]), TypeRegistery::get_builtin_type(BOOL));
}

void SageInterpreter::execute_ltcomp(vector<SageValue> operands) {
    bool first_operator_is_float = (operands[0].valuetype->identify() == F32 || operands[0].valuetype->identify() == F64);
    bool second_operator_is_float = (operands[1].valuetype->identify() == F32 || operands[1].valuetype->identify() == F64);
    if (first_operator_is_float || second_operator_is_float) {
        registers[21] = SageValue(8, operands[0].as_float() < operands[1].as_float(), TypeRegistery::get_builtin_type(BOOL));
    }

    registers[21] = SageValue(8, operands[0].as_i32() < operands[1].as_i32(), TypeRegistery::get_builtin_type(BOOL));
}

void SageInterpreter::execute_gtcomp(vector<SageValue> operands) {
    bool first_operator_is_float = (operands[0].valuetype->identify() == F32 || operands[0].valuetype->identify() == F64);
    bool second_operator_is_float = (operands[1].valuetype->identify() == F32 || operands[1].valuetype->identify() == F64);
    if (first_operator_is_float || second_operator_is_float) {
        registers[21] = SageValue(8, operands[0].as_float() > operands[1].as_float(), TypeRegistery::get_builtin_type(BOOL));
    }

    registers[21] = SageValue(8, operands[0].as_i32() > operands[1].as_i32(), TypeRegistery::get_builtin_type(BOOL));
}

void SageInterpreter::execute_and(vector<SageValue> operands) {
    registers[21] = SageValue(8,
        (operands[0].as_i32() && operands[1].as_i32()) == 1,
        TypeRegistery::get_builtin_type(BOOL));
}

void SageInterpreter::execute_or(vector<SageValue> operands) {
    registers[21] = SageValue(8,
        (operands[0].as_i32() && operands[1].as_i32()) == 1,
        TypeRegistery::get_builtin_type(BOOL));
}

void SageInterpreter::execute_not(vector<SageValue> operands) {
    int value = unpack_int(operands[0]);
    registers[21] = pack_int(!value);
    registers[21] = SageValue(8,
        (!operands[0].as_i32()),
        TypeRegistery::get_builtin_type(BOOL));
}

void SageInterpreter::execute_syscall() {
    /*int callcode = unpack_int(registers[22]);*/
    // TODO:
    // syscall(long(callcode),
    //         registers[0],
    //         registers[1],
    //         registers[2],
    //         registers[3],
    //         registers[4],
    //         registers[5]);
}

void SageInterpreter::execute() {
    program_pointer = 0;
    registers[STACK_POINTER] = SageValue(64, 0, TypeRegistery::get_builtin_type(I64));

    command current_command;
    vector<SageValue> operands;
    bool vm_running = true;

    while (vm_running && program_pointer < program.size()) {
        if (ErrorLogger::get().has_errors()) {
            ErrorLogger::get().report_errors();
            break;
        }
        current_command = program[program_pointer];

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
                break;
            case OP_RET:
                execute_return();
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
                push_stack_scope();
            case OP_NOP:
                break;
            case VOP_EXIT:
                vm_running = false;
                continue;
            default:
                ErrorLogger::get().log_internal_error(
                    "interpreter.cpp",
                    current_linenum,
                    "VM tried to execute unrecognized bytecode");
                vm_running = false;
                continue;
        }

        program_pointer++;
    }
}

int SageInterpreter::store_in_heap(SageValue value) {
    int pointer = heap.size();
    heap[pointer] = value;
    return pointer;
}

void SageInterpreter::close() {
    // deinit stackframe when done with interpreter
    if (frame_pointer != nullptr) {
        if (frame_pointer->previous_frame == nullptr) {
            delete frame_pointer;
            return;
        }

        StackFrame* above = nullptr;
        StackFrame* walk = frame_pointer;
        while (walk->previous_frame != nullptr) {
            above = walk;
            walk = walk->previous_frame;
            delete above;
        }
    }
}











