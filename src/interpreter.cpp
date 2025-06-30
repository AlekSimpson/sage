#include "../include/interpeter.h"
#include "../include/sage_types.h"
#include "../include/node_manager.h"


// StackFrame

StackFrame::StackFrame() {}

void StackFrame::add_allocation(string name) {
    stack_height++;
    string current_scope = current_scope_stack.top();
    allocations[name] = stack_height - current_scope;
}

offset_t StackFrame::get_allocation_offset(string name) {
    if (allocations[name] > stack_height) {
        // ERROR!! ATTEMPTING TO REFERENCE FREED STACK VARIABLE 
        return -1;
    }

    return allocations[name];
}

void StackFrame::push_stack_scope(string scopename) {
    function_scopes[scopename] = stack_height + 1;
    current_scope_stack.push(scopename);
}

void StackFrame::pop_stack_scope() {
    string scopename = current_scope_stack.top();
    current_scope_stack.pop();

    int difference = stack_height - function_scopes[scopename];
    stack_height = stack_height - difference;
    function_scopes.erase(scopename);
}


// Interpreter

SageInterpreter::SageInterpreter() {}

SageInterpreter::SageInterpreter(int stack_size) 
    : stack_frame(StackFrame()) {
    stack.reserve(stack_size);
}

void SageInterpreter::load_program(bytecode _program) {
    program = _program;
}

vector<int> SageInterpreter::dereference_map(instruction* inst, int map[4]) {
    vector<int> return_value = inst->read();

    int counter = -1;
    for (auto value : values) {
        counter++;
        if (map[counter] == 0) {
            continue
        }

        // otherwise dereference register
        return_value[counter] = registers[value];
    }

    return return_values;
}

void SageInterpreter::execute_add(vector<int> operands) {
    if (operands.size() < 3) {
        printf("PANIC(SageInterpreter/execute_add) : execution requires 3 operands but less were found!\n");
    }

    int target_register = operands[0];
    registers[target_register] = operands[1] + operands[2];
}

void SageInterpreter::execute_sub(vector<int> operands) {
    if (operands.size() < 3) {
        printf("PANIC(SageInterpreter/execute_sub) : execution requires 3 operands but less were found!\n");
    }

    int target_register = operands[0];
    registers[target_register] = operands[1] - operands[2];
}

void SageInterpreter::execute_mul(vector<int> operands) {
    if (operands.size() < 3) {
        printf("PANIC(SageInterpreter/execute_mul) : execution requires 3 operands but less were found!\n");
    }

    int target_register = operands[0];
    registers[target_register] = operands[1] * operands[2];
}

void SageInterpreter::execute_div(vector<int> operands) {
    if (operands.size() < 3) {
        printf("PANIC(SageInterpreter/execute_div) : execution requires 3 operands but less were found!\n");
    }

    if (operands[2] == 0) {
        printf("PANIC(SageInterpreter/execute_div) : div cannot divide by 0!\n");
        return;
    }

    int target_register = operands[0];

    if (operands[1] % operands[2] != 0) {
        float float_value = operands[1] / operands[2];
        int heap_pointer = store_in_heap(float_value);
        registers[target_register] = heap_pointer;
        return;
    } 
    registers[target_register] = operands[1] / operands[2];
}

void SageInterpreter::execute_load(vector<int> operands) {
    if (operands.size() < 2) {
        printf("PANIC(SageInterpreter/execute_load): execution expects 2 operands but found less!\n");
    }

    int load_address = registers[STACK_POINTER] + operands[1];
    registers[operand[0]] = stack.at(load_address).load();
}

void SageInterpreter::execute_store(vector<int> operands) {
    if (operands.size() < 2) {
        printf("PANIC(SageInterpreter/execute_store): execution expects 2 operands but found less!\n");
    }

    int store_address = registers[STACK_POINTER] + operands[1];
    stack[store_address] = operands[0]; // TODO: probably should make this a function to store into the stack so that the value to store is stored as SageValue
}

void SageInterpreter::execute_mov(vector<int> operands) {
    if (operands.size() < 2) {
        printf("PANIC(SageInterpreter/execute_mov): execution expects 2 operands but found less!\n");
    }

    registers[operands[1]] = operands[0];
}

void SageInterpreter::execute_call(vector<int> operands) {
    // program pointer needs to be saved
    // the stack_pointer needs to be saved
}

void SageInterpreter::execute(int begin_program_counter) {
    registers[PROGRAM_POINTER] = begin_program_counter;
    registers[STACK_POINTER] = stack_frame.function_scopes[stack_frame.current_scope_stack.top()];

    command current_command = program[begin_program_counter];
    instruction current_instruction = current_command.instruction;
    int inst_map[4] = current_command.map;

    bool reached_end = false;
    vector<int> operands;

    while (!reached_end) {
        operands = dereference_map(&current_instruction, inst_map);
        switch (current_instruction.opcode) {
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
                break;
            case OP_EQ:
                break;
            case OP_LT:
                break;
            case OP_GT:
                break;
            case OP_AND:
                break;
            case OP_OR:
                break;
            case OP_NOT:
                break;
            case OP_SYSCALL:
                break;
            case OP_LABEL:
                break;
            case OP_END_EXECUTION:
                reached_end = true;
                break;
            case OP_BEGIN_EXECUTION:
            case OP_NOP:
                break;
            default:
                // ERROR
                break;
        }

        registers[PROGRAM_POINTER]++;
        current_instruction = program[registers[PROGRAM_POINTER]].instruction;
        inst_map = program[registers[PROGRAM_POINTER]].map;
    }
}


