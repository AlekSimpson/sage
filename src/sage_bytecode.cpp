#include "../include/sage_bytecode.h"


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

inline uint32_t dpack(uint16_t operand1, uint16 operand2) {
  return (static_cast<uint32_t>(operand1) << 16) | operand2; 
}

inline pair<uint16_t, uint16_t> dunpack(uint32_t packed) {
  return {static_cast<uint16_t>(packed >> 16), static_cast<uint16_t>(packed & 0xFFFF)};
}

inline uint32_t tpack(uint8_t op1, uint8_t op2, uint8_t op3, uint8_t op4=0) {
  return (op1 << 24) | (op2 << 16) | (op3 << 8) | op4;
}

inline triple tunpack(uint32_t packed) {
  return triple{
    static_cast<uint8_t>((packed >> 24) & 0xFF),
    static_cast<uint8_t>((packed >> 16) & 0xFF),
    static_cast<uint8_t>((packed >> 8) & 0xFF)
  };
}
