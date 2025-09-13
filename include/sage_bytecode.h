#pragma once

#include "../include/sage_types.h"
#include <cstdint>
#include <vector>

using namespace std;

// References:
// https://claude.ai/chat/f57f3971-8aae-4fe8-bd52-fb0eb08be8d6
// https://github.com/dannyvankooten/monkey-c-monkey-do/blob/master/src/opcode.h
// https://bob.cs.sonoma.edu/testing/sec-stack.html
// https://en.wikipedia.org/wiki/Bytecode

#define ui8 uint8_t
#define ui16 uint16_t
#define ui32 uint32_t

enum SageOpCode {
    OP_ADD = 0,
    OP_SUB,
    OP_MUL,
    OP_DIV,

    OP_LOAD,
    OP_STORE,
    OP_MOV,

    OP_JMP,
    OP_JZ, // jump if zero
    OP_JNZ, // jump if not zero
    OP_CALL, // call a routine
    OP_RET, // return

    OP_EQ,
    OP_LT,
    OP_GT,

    OP_AND,
    OP_OR,
    OP_NOT,
    OP_NOP,

    OP_SYSCALL,
    OP_LABEL,

    VOP_EXIT,
};

struct _double {
    ui16 one;
    ui16 two;
};

struct _triple {
    ui8 one;
    ui8 two;
    ui8 three;
};

struct operand {
    SageValue value;
};

struct instruction {
    SageOpCode opcode = SageOpCode::OP_NOP;
    uint32_t operands = 0;

    instruction();
    instruction(SageOpCode, uint32_t);
    instruction(SageOpCode, int, int);
    instruction(SageOpCode, int, int, int);
    instruction(SageOpCode, int, int, int, int);

    vector<int> read();
};

struct command {
    instruction inst;
    int deref_map[4] = {0, 0, 0, 0};
    // 0 - neutral, use raw
    // 1 - deref register
    // 2 - deref stack
    // 3 - deref heap

    command();
    command(SageOpCode, uint32_t, int [4]);
    command(SageOpCode, int, int, int [4]);
    command(SageOpCode, int, int, int, int [4]);
    command(SageOpCode, int, int, int, int, int [4]);

    string print();
};

typedef vector<command> bytecode;

/* packers */
inline ui32 dpack(ui16 operand1, ui16 operand2);
inline ui32 tpack(ui8 op1, ui8 op2, ui8 op3, ui8 op4 = 0);

/* unpackers */
inline _double dunpack(ui32 packed);
inline _triple tunpack(ui32 packed);
