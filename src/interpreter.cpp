#include "../include/interpeter.h"
#include "../include/sage_types.h"
#include "../include/node_manager.h"

instruction single_op(SageOpCode opcode, int operand) {
    return instruction{opcode, operand, -1, -1};
}

instruction double_op(SageOpCode opcode, int op_one, int op_two) {
    return instruction{opcode, op_one, op_two, -1};
}

instruction triple_op(SageOpCode opcode, int op_one, int op_two, int op_three) {
    return instruction{opcode, op_one, op_two, op_three};
}

SageInterpreter::SageInterpreter() {}

SageInterpreter::SageInterpreter(int _stack_size) {
    program_coiunter = 0;
    stack_pointer = 0;

    stack_size = _stack_size;

    // TODO: load default compiler setting values into memory here
}

string SageInterpreter::get_string(SageArrayValue string_pointer) {
}

void SageInterpreter::load_program(vector<instruction> program) {
}

SageValue* SageInterpreter::run_program() {
}

