#include <sys/syscall.h>  // for SYS_* constants
#include <unistd.h>

#include "../include/interpreter.h"
#include "../include/sage_types.h"
#include "../include/node_manager.h"
#include "../include/registers.h"


// StackFrame

StackFrame::StackFrame(
    StackFrame* previous, map<int, int> caller_cache, int ret_addr, int sp
) : return_address(ret_addr), stack_pointer(sp), previous_frame(previous), 
    saved_caller_values(caller_cache) {}

StackFrame::StackFrame()
    : return_address(-1), stack_pointer(0), previous_frame(nullptr) {}

// Interpreter

SageInterpreter::SageInterpreter() {}

SageInterpreter::SageInterpreter(int stack_size) {
    stack.reserve(stack_size);

    frame_pointer = new StackFrame();
    available_volatiles = {10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
}

void SageInterpreter::push_stack_scope() {
    int32_t return_address = unpack_int(registers[STACK_POINTER]); 
    if (return_address + 1 == stack.size()) {
	// ERROR: STACKOVERFLOW !!!!
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

vector<ui64> SageInterpreter::dereference_map(instruction* inst, int map[4]) {
    vector<int> raw_operands = inst->read();
    vector<ui64> return_values;

    int counter = -1;
    for (int value : raw_operands) {
        counter++;
        if (map[counter] == 0) {
            return_values.push_back(pack_int(value));
            continue;
        }

        // otherwise dereference register
        return_values[counter] = registers[value];
    }

    return return_values;
}

void SageInterpreter::execute_add(vector<ui64> operands) {
    if (operands.size() < 3) {
        printf("PANIC(SageInterpreter/execute_add) : execution requires 3 operands but less were found!\n");
        return;
    }

    int target_register = unpack_int(operands[0]);
    registers[target_register] = pack_int(unpack_int(operands[1]) + unpack_int(operands[2]));
}

void SageInterpreter::execute_sub(vector<ui64> operands) {
    if (operands.size() < 3) {
        printf("PANIC(SageInterpreter/execute_sub) : execution requires 3 operands but less were found!\n");
        return;
    }

    int target_register = unpack_int(operands[0]);
    registers[target_register] = pack_int(unpack_int(operands[1]) - unpack_int(operands[2]));
}

void SageInterpreter::execute_mul(vector<ui64> operands) {
    if (operands.size() < 3) {
        printf("PANIC(SageInterpreter/execute_mul) : execution requires 3 operands but less were found!\n");
        return;
    }

    int target_register = unpack_int(operands[0]);
    registers[target_register] = pack_int(unpack_int(operands[1]) * unpack_int(operands[2]));
}

void SageInterpreter::execute_div(vector<ui64> operands) {
    if (operands.size() < 3) {
        printf("PANIC(SageInterpreter/execute_div) : execution requires 3 operands but less were found!\n");
        return;
    }

    if (operands[2] == 0) {
        printf("PANIC(SageInterpreter/execute_div) : div cannot divide by 0!\n");
        return;
    }

    int target_register = unpack_int(operands[0]);
    registers[target_register] = pack_float(unpack_int(operands[1]) / unpack_int(operands[2]));
}

void SageInterpreter::execute_load(vector<ui64> operands) {
    if (operands.size() < 2) {
        printf("PANIC(SageInterpreter/execute_load): execution expects 2 operands but found less!\n");
        return;
    }

    int load_address = unpack_int(registers[STACK_POINTER]) + unpack_int(operands[1]);
    registers[unpack_int(operands[0])] = stack.at(load_address).load();
}

void SageInterpreter::execute_store(vector<ui64> operands) {
    if (operands.size() < 2) {
        printf("PANIC(SageInterpreter/execute_store): execution expects 2 operands but found less!\n");
        return;
    }

    int offset = unpack_int(operands[1]);
    int store_address = unpack_int(int_reg_inc(registers[STACK_POINTER], offset));
    stack[store_address] = register_to_value(operands[0]);
}

void SageInterpreter::execute_mov(vector<ui64> operands) {
    if (operands.size() < 2) {
        printf("PANIC(SageInterpreter/execute_mov): execution expects 2 operands but found less!\n");
        return;
    }

    int dest = unpack_int(operands[1]);
    registers[dest] = operands[0];
}

void SageInterpreter::execute_call(vector<ui64> operands) {
    push_stack_scope();
    int caller_id_pointer = unpack_int(operands[0]);
    string* caller_id_string = heap[caller_id_pointer].value.string_value;
    program_pointer = procedure_label_encoding[*caller_id_string];
}

void SageInterpreter::execute_return() {
    int callback_addr = frame_pointer->return_address;
    program_pointer = callback_addr;
    pop_stack_scope();
}

void SageInterpreter::execute_eqcomp(vector<ui64> operands) {
    int value_a = unpack_int(operands[0]);
    int value_b = unpack_int(operands[1]);

    registers[21] = pack_int(value_a == value_b);
}

void SageInterpreter::execute_ltcomp(vector<ui64> operands) {
    int value_a = unpack_int(operands[0]);
    int value_b = unpack_int(operands[1]);

    registers[21] = pack_int(value_a < value_b);
}

void SageInterpreter::execute_gtcomp(vector<ui64> operands) {
    int value_a = unpack_int(operands[0]);
    int value_b = unpack_int(operands[1]);

    registers[21] = pack_int(value_a > value_b);
}

void SageInterpreter::execute_and(vector<ui64> operands) {
    int value_a = unpack_int(operands[0]);
    int value_b = unpack_int(operands[1]);
    registers[21] = pack_int((value_a && value_b) == 1);
}

void SageInterpreter::execute_or(vector<ui64> operands) {
    int value_a = unpack_int(operands[0]);
    int value_b = unpack_int(operands[1]);
    registers[21] = pack_int((value_a || value_b) == 1);
}

void SageInterpreter::execute_not(vector<ui64> operands) {
    int value = unpack_int(operands[0]);
    registers[21] = pack_int(!value);
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
    registers[program_pointer] = pack_int(0);
    registers[STACK_POINTER] = pack_int(0);

    command current_command = program[0];

    bool reached_end = false;
    vector<ui64> operands;

    while (!reached_end) {
        operands = dereference_map(&current_command.inst, current_command.map);
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
            case OP_ALLOC:
                break; // TODO:
            case OP_FREE:
                break; // TODO:
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
            case OP_END_EXECUTION:
            case OP_BEGIN_EXECUTION:
            case OP_NOP:
                break;
            default:
                // ERROR
                break;
        }

        program_pointer++;
        current_command = program[program_pointer];
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

bool SageInterpreter::register_is_stale(SageSymbol* symbol, int reg) {
    // this checks if the value at the volatile register spot is the same as the value that this symbol represents

    switch (symbol->value.valuetype->identify()) {
        case BOOL: {
            int contents = unpack_int(registers[reg]);
            if (symbol->value.value.bool_value != contents) {
                // is stale
                available_volatiles.insert(reg);
                return true;
            }

            return false;
        }
        case CHAR: {
            int contents = unpack_int(registers[reg]);
            if (int(symbol->value.value.char_value) != contents) {
                available_volatiles.insert(reg);
                return true;
            }

            return false;
        }
        case I8:
        case I32:
        case I64: {
            int contents = unpack_int(registers[reg]);
            if (symbol->value.value.int_value != contents) {
                available_volatiles.insert(reg);
                return true;
            }

            return false;
        }
        case F32:
        case F64: {
            float contents = unpack_float(registers[reg]);
            if (symbol->value.value.float_value != contents) {
                available_volatiles.insert(reg);
                return true;
            }

            return false;
        }
        case ARRAY: {
            // might need to check if sub type is char first then we can do string compare
            int contents = unpack_int(registers[reg]);
            if (symbol->value.value.int_value != contents) {
                available_volatiles.insert(reg);
                return true;
            }

            return false;
        }
        case VOID:
        case FUNC:
        case POINTER: {
            void* contents = unpack_pointer(registers[reg]);
            if (symbol->value.value.complex_type != contents) {
                available_volatiles.insert(reg);
                return true;
            }

            return false;
        }
        default:
            return true;
    }
}

int SageInterpreter::get_volatile_register() {
    // this finds a stale volatile register, and returns it to be used
    // FIX: this model of volatile register access won't work for codegen
    int available_volatile = *available_volatiles.begin();
    available_volatiles.erase(available_volatile);
    return available_volatile;
}













