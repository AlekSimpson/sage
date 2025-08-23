#include <cstdint>
#include <vector>

#include "../include/sage_bytecode.h"

command::command(SageOpCode code, uint32_t ops, int _map[4]) : inst(instruction(code, ops)) {
  for (int i = 0; i < 4; ++i) {
    map[i] = _map[i];
  }
}

command::command(SageOpCode code, int op1, int op2, int _map[4]) : inst(instruction(code, op1, op2)) {
  for (int i = 0; i < 4; ++i) {
    map[i] = _map[i];
  }
}

command::command(SageOpCode code, int op1, int op2, int op3, int _map[4]) : inst(instruction(code, op1, op2, op3)) {
  for (int i = 0; i < 4; ++i) {
    map[i] = _map[i];
  }
}

command::command(SageOpCode code, int op1, int op2, int op3, int op4, int _map[4]) : inst(instruction(code, op1, op2, op3, op4)) {
  for (int i = 0; i < 4; ++i) {
    map[i] = _map[i];
  }
}

instruction::instruction(SageOpCode code, uint32_t ops) : opcode(code), operands(ops) {};

instruction::instruction(SageOpCode code, int op1, int op2) {
    opcode = code;
    operands = dpack(op1, op2);
}

instruction::instruction(SageOpCode code, int op1, int op2, int op3) {
    opcode = code;
    operands = tpack(op1, op2, op3);
}

instruction::instruction(SageOpCode code, int op1, int op2, int op3, int op4) {
    opcode = code;
    operands = tpack(op1, op2, op3, op4);
}

inline uint32_t dpack(uint16_t operand1, uint16_t operand2) {
  return (static_cast<uint32_t>(operand1) << 16) | operand2; 
}

inline pair<uint16_t, uint16_t> dunpack(uint32_t packed) {
  return {static_cast<uint16_t>(packed >> 16), static_cast<uint16_t>(packed & 0xFFFF)};
}

inline uint32_t tpack(uint8_t op1, uint8_t op2, uint8_t op3, uint8_t op4) {
  return (op1 << 24) | (op2 << 16) | (op3 << 8) | op4;
}

inline triple tunpack(uint32_t packed) {
  return triple{
    static_cast<uint8_t>((packed >> 24) & 0xFF),
    static_cast<uint8_t>((packed >> 16) & 0xFF),
    static_cast<uint8_t>((packed >> 8) & 0xFF)
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
            triple unpacked = tunpack(operands);
            result = {unpacked.one, unpacked.two, unpacked.three};
            break;
        }
        case OP_LOAD:
        case OP_STORE:
        case OP_MOV: {
            // These typically need 2 operands
            pair<uint16_t, uint16_t> unpacked = dunpack(operands);
            result = {static_cast<int>(unpacked.first), static_cast<int>(unpacked.second)};
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

