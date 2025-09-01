#include <cstdint>
#include <vector>
#include <string>
#include <map>

#include "../include/error_logger.h"
#include "../include/sage_bytecode.h"

using namespace std;

command::command() {}

command::command(SageOpCode code, uint32_t ops, int _map[4]) : inst(instruction(code, ops)) {
    for (int i = 0; i < 4; ++i) {
        deref_map[i] = _map[i];
    }
}

command::command(SageOpCode code, int op1, int op2, int _map[4]) : inst(instruction(code, op1, op2)) {
    for (int i = 0; i < 4; ++i) {
        deref_map[i] = _map[i];
    }
}

command::command(SageOpCode code, int op1, int op2, int op3, int _map[4]) : inst(instruction(code, op1, op2, op3)) {
    for (int i = 0; i < 4; ++i) {
        deref_map[i] = _map[i];
    }
}

command::command(SageOpCode code, int op1, int op2, int op3, int op4, int _map[4]) : inst(
    instruction(code, op1, op2, op3, op4)) {
    for (int i = 0; i < 4; ++i) {
        deref_map[i] = _map[i];
    }
}

string command::print() {
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

    union operand_packer {
        _double d;
        _triple t;
        ui32 s;
    };

    // no operands: RET, NOP, SYSCALL
    operand_packer packer;
    bool using_single = false;
    bool using_double = false;
    bool using_none = false;
    switch (inst.opcode) {
        case OP_MOV:
        case OP_LOAD:
        case OP_STORE:
        case OP_AND:
        case OP_OR:
        case OP_EQ:
        case OP_LT:
        case OP_GT:
        case OP_JZ:
        case OP_JNZ:
            using_double = true;
            packer.d = dunpack(inst.operands);
            break;

        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
            packer.t = tunpack(inst.operands);
            break;

        case OP_NOT:
        case OP_JMP:
        case OP_CALL:
        case OP_LABEL:
            using_single = true;
            packer.s = inst.operands;
            break;

        default:
            using_none = true;
            break;
    }

    if (using_none) {
        return sen(opcode_map[inst.opcode]);
    }
    if (using_single) {
        string op1 = to_string(packer.s);
        if (deref_map[0] == 1) {
            op1 = str("r", to_string(packer.s));
        }
        if (inst.opcode == OP_JMP || inst.opcode == OP_CALL || inst.opcode == OP_LABEL) {
            op1 = str("@", to_string(packer.s));
        }

        return sen(opcode_map[inst.opcode], op1);
    }
    if (using_double) {
        string op1 = to_string(packer.d.one);
        if (deref_map[0] == 1 || inst.opcode == OP_LOAD) {
            op1 = str("r", op1);
            // op1 = "r" + op1;
        }

        string op2 = to_string(packer.d.two);
        if (deref_map[1] == 1 || inst.opcode == OP_MOV) {
            op2 = str("r", to_string(packer.d.two));
        }else if (inst.opcode == OP_LOAD || inst.opcode == OP_STORE) {
            op2 = str("+", to_string(packer.d.two));
        }else if (inst.opcode == OP_JZ || inst.opcode == OP_JNZ) {
            op2 = str("@", to_string(packer.d.two));
        }

        return sen(opcode_map[inst.opcode], op1, op2);
    }

    string op1 = to_string(packer.t.one);
    if (deref_map[0] == 1 || inst.opcode == OP_ADD
                          || inst.opcode == OP_SUB
                          || inst.opcode == OP_MUL
                          || inst.opcode == OP_DIV) {
        op1 = str("r", to_string(packer.t.one));
    }

    string op2 = to_string(packer.t.two);
    if (deref_map[1] == 1) {
        op2 = str("r", to_string(packer.t.two));
    }

    string op3 = to_string(packer.t.three);
    if (deref_map[2] == 1) {
        op3 = str("r", to_string(packer.t.three));
    }

    return sen(opcode_map[inst.opcode], op1, op2, op3);
}

instruction::instruction() {}

instruction::instruction(SageOpCode code, uint32_t ops) : opcode(code), operands(ops) {
}

instruction::instruction(SageOpCode code, int op1, int op2) {
    opcode = code;
    operands = dpack(static_cast<ui16>(op1), static_cast<ui16>(op2));
}

instruction::instruction(SageOpCode code, int op1, int op2, int op3) {
    opcode = code;
    //operands = tpack(op1, op2, op3);
    operands = tpack(static_cast<ui16>(op1), static_cast<ui16>(op2), static_cast<ui16>(op3));
}

instruction::instruction(SageOpCode code, int op1, int op2, int op3, int op4) {
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

vector<int> instruction::read() {
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
