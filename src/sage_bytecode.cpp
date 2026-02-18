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
        {OP_ADD, "ADD"},
        {OP_SUB, "SUB"},
        {OP_MUL, "MUL"},
        {OP_DIV, "DIV"},
        {OP_LOAD, "LOAD"},
        {OP_STORE, "STORE"},
        {OP_MOV, "MOV"},
        {OP_JMP, "JMP"},
        {OP_JZ, "JZ"},
        {OP_JNZ, "JNZ"},
        {OP_CALL, "CALL"},
        {OP_RET, "RET"},
        {OP_EQ, "EQ"},
        {OP_LT, "LT"},
        {OP_GT, "GT"},
        {OP_AND, "AND"},
        {OP_OR, "OR"},
        {OP_NOT, "NOT"},
        {OP_NOP, "NOP"},
        {OP_SYSCALL, "SYSCALL"},
        {OP_LABEL, "LABEL"},
        {VOP_EXIT, "EXIT"},
        {OP_FADD, "FADD"},
        {OP_FSUB, "FSUB"},
        {OP_FMUL, "FMUL"},
        {OP_FDIV, "FDIV"},
        {OP_ALLOC, "ALLOC"},
        {OP_FMOV, "FMOV"},
        {OP_ITF_MOV, "ITFMOV"},
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

        case OP_LOAD: {
            string operand_string_1 = to_string(operands[0]);
            string operand_string_2 = str("r", to_string(operands[1]));
            string operand_string_3 = str("$", to_string(operands[2]));
            return sen(opcode_map[instruction.opcode], operand_string_1, operand_string_2, operand_string_3);
        }

        case OP_STORE: {
            string operand_string_1 = to_string(operands[0]);
            string operand_string_2 = str("($fp - ", to_string(operands[1]), ")");
            string operand_string_3 = address_mode[1] == 1 ? str("r", to_string(operands[2])) : to_string(operands[2]);
            return sen(opcode_map[instruction.opcode], operand_string_1, operand_string_2, operand_string_3);
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
            return sen("memcpy", operand_string_1, operand_string_2, operand_string_3);
        }
        case OP_STATIC_COPY: {
            string operand_string_1 = to_string(operands[0]);
            string operand_string_2 = address_mode[0] == 1 ? str("r", to_string(operands[1])) : to_string(operands[1]);
            string operand_string_3 = address_mode[1] == 1 ? str("r", to_string(operands[2])) : to_string(operands[2]);
            return sen("statcpy", operand_string_1, operand_string_2, operand_string_3);
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
