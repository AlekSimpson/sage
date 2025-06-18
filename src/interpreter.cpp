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

void SageInterpreter::execute(int begin_program_counter) {
    registers[PROGRAM_POINTER] = begin_program_counter;
    registers[STACK_POINTER] = stack_frame.function_scopes[stack_frame.current_scope_stack.top()];

    instruction current_instruction;

    bool reached_end = false;

    while (!reached_end) {
        switch (current_instruction.opcode) {
            case OP_ADD:
                break;
            case OP_SUB:
                break;
            case OP_MUL:
                break;
            case OP_DIV:
                break;
            case OP_LOAD:
                break;
            case OP_STORE:
                break;
            case OP_MOV:
                break;
            case OP_ALLOC:
                break;
            case OP_FREE:
                break;
            case OP_JMP:
                break;
            case OP_JZ:
                break;
            case OP_JNZ:
                break;
            case OP_CALL:
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
            case OP_BEGIN_EXECUTION:
                break;
            case OP_END_EXECUTION:
                break;
            default:
                // ERROR
                break;
        }
    }
}


