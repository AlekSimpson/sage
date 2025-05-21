#pragma once

#include "sage_types.h"
#include "node_manager.h"
#include "codegen.h"

// References:
// https://claude.ai/chat/760d2ffe-a186-4dff-8f16-6ecb05fb5b78
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
  OP_RV,   // return value

  OP_EQ,
  OP_LT,
  OP_GT,

  OP_AND,
  OP_OR,
  OP_NOT,

  OP_PRINT,
  OP_READ
};

struct instruction {
  SageOpCode opcode;
  int operand_one;
  int operand_two;
  int operand_three;
};

instruction single_op(SageOpCode, int);
instruction double_op(SageOpCode, int, int);
instruction triple_op(SageOpCode, int, int, int);

class SageInterpreter {
public:
  int program_counter;
  int stack_pointer; // always points to the value on the top of the stack
  
  int stack_size;

  // sage registers
  int sr1 = -1;
  int sr2 = -1;
  int sr3 = -1;
  int sr4 = -1;
  int sr5 = -1;
  int sr6 = -1;
  int sr7 = -1;
  int sr8 = -1;
  int sr9 = -1;
  int sra = -1; // bool logical result register
  int srb = -1; // syscall register
  int src = -1; // subroutine return register

  vector<instruction> program;
  vector<SageValue&> heap;
  SageValue&* stack;
  SageCompiler* compiler;

  SageInterpreter();
  SageInterpreter(SageCompiler*, int stack_size);

  SageSymbolTable symbol_table();
  string get_string(SageArrayValue);
  void load_program(vector<instruction> program);
  SageValue* run_program();
};








