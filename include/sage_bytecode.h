#pragma once

// References:
// https://claude.ai/chat/f57f3971-8aae-4fe8-bd52-fb0eb08be8d6
// https://github.com/dannyvankooten/monkey-c-monkey-do/blob/master/src/opcode.h
// https://bob.cs.sonoma.edu/testing/sec-stack.html
// https://en.wikipedia.org/wiki/Bytecode

// NOTE: this is not a complete set of opcodes, I am planning to just add them as needed for now
enum SageOpCode {
  OP_PUSH = 0,
  OP_POP,

  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,

  OP_LOAD,
  OP_STORE,
  OP_MOV,

  // heap ops
  OP_ALLOC,
  OP_FREE,

  OP_JMP,
  OP_JZ,   // jump if zero
  OP_JNZ,  // jump if not zero
  OP_CALL, // call a routine
  OP_RET,  // return

  OP_EQ,
  OP_LT,
  OP_GT,

  OP_AND,
  OP_OR,
  OP_NOT,

  OP_SYSCALL,
  OP_LABEL,
  
  OP_BEGIN_EXECUTION, // compile time execution op codes, these tell the interpreter to actually begin interprettting the instructions that come in as opposed to simply accumulating program instructions
  OP_END_EXECUTION // tells the interpreter to pause execution
};

typedef vector<instruction> bytecode;

struct triple {
  uint8_t one;
  uint8_t two;
  uint8_t three;
};

struct instruction {
  SageOpCode opcode;
  uint32_t   operands;

  instruction(SageOpCode, uint32_t);
  instruction(SageOpCode, int, int);
  instruction(SageOpCode, int, int, int);
  instruction(SageOpCode, int, int, int, int);
};

inline uint32_t dpack(uint16_t operand1, uint16 operand2);

inline pair<uint16_t, uint16_t> dunpack(uint32_t packed);

inline uint32_t tpack(uint8_t op1, uint8_t op2, uint8_t op3, uint8_t op4=0);

inline triple tunpack(uint32_t packed);
