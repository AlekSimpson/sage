#include <cstdint>
#include <vector>
#include <string>
#include <map>

#include "../include/error_logger.h"
#include "../include/sage_bytecode.h"

using namespace std;

Command::Command() {
}

Command::Command(SageOpCode code, uint32_t operand, AddressMode mode) : instruction(Instruction(code, operand)) {
    for (int i = 0; i < 2; ++i) {
        address_mode[i] = mode[i];
    }
}

Command::Command(SageOpCode code, int op1, int op2, AddressMode mode) : instruction(Instruction(code, op1, op2)) {
    for (int i = 0; i < 2; ++i) {
        address_mode[i] = mode[i];
    }
}

Command::Command(SageOpCode code, int op1, int op2, int op3, AddressMode mode) : instruction(Instruction(code, op1, op2, op3)) {
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
        {VOP_EXIT, "EXIT"}
    };

    if (instruction.opcode == OP_RET ||
        instruction.opcode == OP_NOP ||
        instruction.opcode == OP_SYSCALL ||
        instruction.opcode == VOP_EXIT) {
        return opcode_map[instruction.opcode];
    }

    switch (instruction.opcode) {
        case OP_NOT: {
            return sen(opcode_map[instruction.opcode], str("r", to_string(instruction.operands)));
        }
        case OP_LABEL:
        case OP_JMP:
        case OP_CALL: {
            auto it = label_names->find(instruction.operands);
            string operand = str("@", to_string(instruction.operands), " (", it->second, ")");
            return sen(opcode_map[instruction.opcode], operand);
        }

        case OP_MOV: {
            _double operands = dunpack(instruction.operands);
            string operand_string_1 = str("r", to_string(operands.one));
            string operand_string_2 = address_mode[1] == 1 ? str("r", to_string(operands.two)) : to_string(operands.two);
            return sen(opcode_map[instruction.opcode], operand_string_1, operand_string_2);
        }

        case OP_LOAD: {
            _double operands = dunpack(instruction.operands);
            string operand_string_1 = str("r", to_string(operands.one));
            string operand_string_2 = str("($fp + ", to_string(operands.two), ")");
            return sen(opcode_map[instruction.opcode], operand_string_1, operand_string_2);
        }

        case OP_STORE: {
            _double operands = dunpack(instruction.operands);
            string operand_string_1 = str("($fp + ", to_string(operands.one), ")");
            string operand_string_2 = address_mode[1] == 1 ? str("r", to_string(operands.two)) : to_string(operands.two);
            return sen(opcode_map[instruction.opcode], operand_string_1, operand_string_2);
        }

        case OP_EQ:
        case OP_LT:
        case OP_GT:
        case OP_AND:
        case OP_OR: {
            // two operands
            _double operands = dunpack(instruction.operands);
            string operand_string_1 = address_mode[0] == 1 ? str("r", to_string(operands.one)) : to_string(operands.one);
            string operand_string_2 = address_mode[1] == 1 ? str("r", to_string(operands.two)) : to_string(operands.two);
            return sen(opcode_map[instruction.opcode], operand_string_1, operand_string_2);
        }

        case OP_JZ:
        case OP_JNZ: {
            _double operands = dunpack(instruction.operands);
            string operand_string_1 = str("r", to_string(operands.one));

            auto it = label_names->find(operands.two);
            string operand_string_2 = str("@", to_string(operands.two), " (", it->second, ")");
            return sen(opcode_map[instruction.opcode], operand_string_1, operand_string_2);
        }

        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV: {
            _triple operands = tunpack(instruction.operands);
            string operand_string_1 = str("r", to_string(operands.one));
            string operand_string_2 = address_mode[0] == 1 ? str("r", to_string(operands.two)) : to_string(operands.two);
            string operand_string_3 = address_mode[1] == 1 ? str("r", to_string(operands.three)) : to_string(operands.three);
            return sen(opcode_map[instruction.opcode], operand_string_1, operand_string_2, operand_string_3);
        }

        default:
            return "Unknown bytecode";
    }
}

Instruction::Instruction() {
}

Instruction::Instruction(SageOpCode code, uint32_t ops) : opcode(code), operands(ops) {
}

Instruction::Instruction(SageOpCode code, int op1, int op2) {
    opcode = code;
    operands = dpack(static_cast<ui16>(op1), static_cast<ui16>(op2));
}

Instruction::Instruction(SageOpCode code, int op1, int op2, int op3) {
    opcode = code;
    //operands = tpack(op1, op2, op3);
    operands = tpack(static_cast<ui16>(op1), static_cast<ui16>(op2), static_cast<ui16>(op3));
}

Instruction::Instruction(SageOpCode code, int op1, int op2, int op3, int op4) {
    opcode = code;
    operands = tpack(op1, op2, op3, op4);
}

inline ui32 dpack(ui16 operand1, ui16 operand2) {
    return (static_cast<ui32>(operand1) << 16) | operand2;
}

inline _double dunpack(ui32 packed) {
    return {static_cast<ui16>(packed >> 16), static_cast<ui16>(packed & 0xFFFF)};
}

inline ui32 tpack(ui8 op1, ui8 op2, ui8 op3, ui8 op4) {
    return (op1 << 24) | (op2 << 16) | (op3 << 8) | op4;
}

inline _triple tunpack(ui32 packed) {
    return _triple{
        static_cast<ui8>((packed >> 24) & 0xFF),
        static_cast<ui8>((packed >> 16) & 0xFF),
        static_cast<ui8>((packed >> 8) & 0xFF)
    };
}

vector<int> Instruction::unpack_instruction() {
    vector<int> result;

    switch (opcode) {
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_AND:
        case OP_OR:
        case OP_EQ:
        case OP_LT:
        case OP_GT: {
            // These typically need 3 operands (dest, src1, src2)
            _triple unpacked = tunpack(operands);
            result = {unpacked.one, unpacked.two, unpacked.three};
            break;
        }
        case OP_LOAD:
        case OP_STORE:
        case OP_MOV: {
            // These typically need 2 operands
            _double unpacked = dunpack(operands);
            result = {static_cast<int>(unpacked.one), static_cast<int>(unpacked.two)};
            break;
        }
        default: {
            // Single operand
            result = {static_cast<int>(operands)};
            break;
        }
    }

    return result;
}
