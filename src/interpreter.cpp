#include "../include/interpeter.h"
#include "../include/sage_types.h"
#include "../include/node_manager.h"


// StackFrame

StackFrame::StackFrame() : stack_height(0) {}

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

SageInterpreter::SageInterpreter(int _stack_size) 
    : program_counter(0), stack_size(_stack_size), stack_frame(StackFrame()) {
    for (int i = 0; i < stack_size; ++i) {
        stack[i] = SageIntValue(0, 64);
    }
}

string SageInterpreter::get_string(SageArrayValue string_pointer) {}

void SageInterpreter::load_program(vector<instruction> program) {}

SageValue* SageInterpreter::run_program() {}

int SageInterpreter::allocate_register() {

}

