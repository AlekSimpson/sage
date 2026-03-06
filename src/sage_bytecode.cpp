#include <cstdint>
#include <vector>
#include <cstring>
#include <string>
#include <map>

#include "../include/error_logger.h"
#include "../include/sage_bytecode.h"

using namespace std;

AddressMode operator+(AddressMode &operand_a, AddressMode &operand_b) {
    return {operand_a[0] + operand_b[0], operand_a[1] + operand_b[1]};
}

Command::Command() {
}

Command::Command(SageOpCode code, int64_t operand, AddressMode mode) : instruction(Instruction(code, operand)) {
    for (int i = 0; i < 2; ++i) {
        address_mode[i] = mode[i];
    }
}

Command::Command(SageOpCode code, int64_t op1, int64_t op2, AddressMode mode) : instruction(Instruction(code, op1, op2)) {
    for (int i = 0; i < 2; ++i) {
        address_mode[i] = mode[i];
    }
}

Command::Command(SageOpCode code, int64_t op1, int64_t op2, int64_t op3, AddressMode mode) : instruction(Instruction(code, op1, op2, op3)) {
    for (int i = 0; i < 2; ++i) {
        address_mode[i] = mode[i];
    }
}

string Command::print(const map<int, string> *label_names) {
    map<SageOpCode, string> opcode_map = {
        {OP_ADD, "add"},
        {OP_SUB, "sub"},
        {OP_MUL, "mul"},
        {OP_DIV, "div"},
        {OP_LOAD, "load"},
        {OP_STORE, "store"},
        {OP_MOV, "mov"},
        {OP_JMP, "jmp"},
        {OP_JZ, "jz"},
        {OP_JNZ, "jnz"},
        {OP_CALL, "call"},
        {OP_RET, "ret"},
        {OP_EQ, "eq"},
        {OP_LT, "lt"},
        {OP_GT, "gt"},
        {OP_AND, "and"},
        {OP_OR, "or"},
        {OP_NOT, "not"},
        {OP_NOP, "nop"},
        {OP_SYSCALL, "syscall"},
        {OP_LABEL, "label"},
        {VOP_EXIT, "exit"},
        {OP_FADD, "fadd"},
        {OP_FSUB, "fsub"},
        {OP_FMUL, "fmul"},
        {OP_FDIV, "fdiv"},
        {OP_ALLOC, "allc"},
        {OP_FMOV, "fmov"},
        {OP_ITF_MOV, "itfmov"},
        {OP_LOADR, "loadr"},
        {OP_LOADP, "loadp"},
        {OP_LOADA, "loada"}
    };

    auto &operands = instruction.operands;
    switch (instruction.opcode) {
        case OP_RET:
        case OP_NOP:
        case OP_SYSCALL:
        case VOP_EXIT:
            return opcode_map[instruction.opcode];
        case OP_NOT: {
            return sen(opcode_map[instruction.opcode], str("r", to_string(operands[0])));
        }
        case OP_LABEL:
        case OP_JMP:
        case OP_CALL: {
            if (label_names != nullptr) {
                auto it = label_names->find(operands[0]);
                string operand = str("@", to_string(operands[0]), " (", it->second, ")");
                return sen(opcode_map[instruction.opcode], operand);
            }
            string operand = str("@", to_string(operands[0]));
            return sen(opcode_map[instruction.opcode], operand);
        }

        case OP_MOV: {
            string operand_string_1 = str("r", to_string(operands[0]));
            string operand_string_2 = address_mode[1] == 1 ? str("r", to_string(operands[1])) : to_string(operands[1]);
            return sen(opcode_map[instruction.opcode], operand_string_1, operand_string_2);
        }
        case OP_FMOV: {
            string operand_string_1 = str("fr", to_string(operands[0]));
            double immediate_operand = 0.0;
            std::memcpy(&immediate_operand, &operands[1], sizeof(double));
            string operand_string_2 = address_mode[1] == 1 ? str("fr", to_string(operands[1])) : to_string(immediate_operand);
            return sen(opcode_map[instruction.opcode], operand_string_1, operand_string_2);
        }

        case OP_ITF_MOV: {
            string operand_string_1 = str("fr", to_string(operands[0]));
            string operand_string_2 = str("r", to_string(operands[1]));
            return sen(opcode_map[instruction.opcode], operand_string_1, operand_string_2);
        }

        case OP_LOADP:
        case OP_LOADA:
        case OP_LOAD: {
            string operand_string_0 = to_string(operands[0]);
            string operand_string_1 = str("r", to_string(operands[1]));
            string operand2 = address_mode[1] == 1 ? str("r", to_string(operands[2])) : to_string(operands[2]);
            string operand_string_2 = str("($fp - ", operand2, ")");
            return sen(opcode_map[instruction.opcode], operand_string_0, operand_string_1, operand_string_2);
        }
        case OP_LOADR: {
            string operand_string_0 = str("r", to_string(operands[0]));
            string operand1 = address_mode[1] == 1 ? str("r", to_string(operands[1])) : to_string(operands[1]);
            string operand_string_1 = str("($fp - ", operand1, ")");
            return sen(opcode_map[instruction.opcode], operand_string_0, operand_string_1);
        }

        case OP_STORE: {
            string operand_string_1 = to_string(operands[0]);
            string operand_string_2 = str("($fp - ", to_string(operands[1]), ")");
            string operand_string_3 = address_mode[1] == 1 ? str("r", to_string(operands[2])) : to_string(operands[2]);
            return sen(opcode_map[instruction.opcode], operand_string_1, operand_string_2, operand_string_3);
        }
        case OP_STOREA: {
            string operand_string_1 = to_string(operands[0]);
            string operand_string_2 = address_mode[0] == 1 ? str("$r", to_string(operands[1])) : str("$", to_string(operands[1]));
            string operand_string_3 = address_mode[1] == 1 ? str("r", to_string(operands[2])) : to_string(operands[2]);
            return sen("storea", operand_string_1, operand_string_2, operand_string_3);
        }

        case OP_EQ:
        case OP_LT:
        case OP_GT:
        case OP_AND:
        case OP_OR: {
            // two operands
            string operand_string_1 = address_mode[0] == 1 ? str("r", to_string(operands[0])) : to_string(operands[0]);
            string operand_string_2 = address_mode[1] == 1 ? str("r", to_string(operands[1])) : to_string(operands[1]);
            return sen(opcode_map[instruction.opcode], operand_string_1, operand_string_2);
        }

        case OP_JZ:
        case OP_JNZ: {
            string operand_string_1 = str("r", to_string(operands[0]));

            if (label_names != nullptr) {
                auto it = label_names->find(operands[1]);
                string operand_string_2 = str("@", to_string(operands[1]), " (", it->second, ")");
                return sen(opcode_map[instruction.opcode], operand_string_1, operand_string_2);
            }

            string operand_string_2 = str("@", to_string(operands[1]));
            return sen(opcode_map[instruction.opcode], operand_string_1, operand_string_2);
        }

        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV: {
            string operand_string_1 = str("r", to_string(operands[0]));
            string operand_string_2 = address_mode[0] == 1 ? str("r", to_string(operands[1])) : to_string(operands[1]);
            string operand_string_3 = address_mode[1] == 1 ? str("r", to_string(operands[2])) : to_string(operands[2]);
            return sen(opcode_map[instruction.opcode], operand_string_1, operand_string_2, operand_string_3);
        }
        case OP_FADD:
        case OP_FSUB:
        case OP_FMUL:
        case OP_FDIV: {
            string operand_string_1 = str("fr", to_string(operands[0]));
            string operand_string_2 = address_mode[0] == 1 ? str("fr", to_string(operands[1])) : to_string(operands[1]);
            string operand_string_3 = address_mode[1] == 1 ? str("fr", to_string(operands[2])) : to_string(operands[2]);
            return sen(opcode_map[instruction.opcode], operand_string_1, operand_string_2, operand_string_3);
        }

        case OP_ALLOC: {
            return sen(opcode_map[instruction.opcode], to_string(operands[0]));
        }

        case OP_MEMCPY: {
            string operand_string_1 = to_string(operands[0]);
            string operand_string_2 = address_mode[0] == 1 ? str("r", to_string(operands[1])) : to_string(operands[1]);
            string operand_string_3 = address_mode[1] == 1 ? str("r", to_string(operands[2])) : to_string(operands[2]);
            return sen("mcpy", operand_string_1, operand_string_2, operand_string_3);
        }
        case OP_ADDR_MEMCPY: {
            string operand_string_1 = to_string(operands[0]);
            string operand_string_2 = address_mode[0] == 1 ? str("r", to_string(operands[1])) : to_string(operands[1]);
            string operand_string_3 = address_mode[1] == 1 ? str("r", to_string(operands[2])) : to_string(operands[2]);
            return sen("acpy", operand_string_1, operand_string_2, operand_string_3);
        }
        case OP_STATIC_COPY: {
            string operand_string_1 = to_string(operands[0]);
            string operand_string_2 = address_mode[0] == 1 ? str("r", to_string(operands[1])) : to_string(operands[1]);
            string operand_string_3 = address_mode[1] == 1 ? str("r", to_string(operands[2])) : to_string(operands[2]);
            return sen("scpy", operand_string_1, operand_string_2, operand_string_3);
        }
        default:
            return sen("Unknown bytecode", instruction.opcode);
    }
}

Instruction::Instruction() {
}

Instruction::Instruction(SageOpCode code, int64_t op1)
    : opcode(code), operands({op1, 0, 0}), operand_count(1) {
}

Instruction::Instruction(SageOpCode code, int64_t op1, int64_t op2)
    : opcode(code), operands({op1, op2, 0}), operand_count(2) {
}

Instruction::Instruction(SageOpCode code, int64_t op1, int64_t op2, int64_t op3)
    : opcode(code), operands({op1, op2, op3}), operand_count(3) {
}
