#pragma once

#include "../include/sage_types.h"
#include <cstdint>
#include <vector>
#include <map>
#include <array>

using namespace std;

// References:
// https://claude.ai/chat/f57f3971-8aae-4fe8-bd52-fb0eb08be8d6
// https://github.com/dannyvankooten/monkey-c-monkey-do/blob/master/src/opcode.h
// https://bob.cs.sonoma.edu/testing/sec-stack.html
// https://en.wikipedia.org/wiki/Bytecode

#define ui8 uint8_t
#define ui16 uint16_t
#define ui32 uint32_t

using AddressMode = std::array<int, 2>;
inline AddressMode _00 = {0, 0};
inline AddressMode _10 = {1, 0};
inline AddressMode _01 = {0, 1};
inline AddressMode _11 = {1, 1};

AddressMode operator+(AddressMode &, AddressMode &);

enum SageOpCode {
    // op (operand) = could be referencing immediate value or value at registers[op]

    OP_ADD = 0, // _xx | add reg, op, op
    OP_SUB,     // _xx | sub reg, op, op
    OP_MUL,     // _xx | mul reg, op, op
    OP_DIV,     // _xx | div reg, op, op

    OP_FADD,    // _xx | fadd freg, op, op
    OP_FSUB,    // _xx | fsub freg, op, op
    OP_FMUL,    // _xx | fmul freg, op, op
    OP_FDIV,    // _xx | fdiv freg, op, op

    OP_STATIC_COPY, // _xx | statcpy bytes, ($fp, - op), $static_pointer
    OP_MEMCPY,      // _xx | memcpy bytes, ($fp - op), ($fp - op)
    OP_ALLOC,       // _00 | alloc bytesize
    OP_LOAD,        // _00 | load bytes, reg, ($fp - offset)
    OP_STORE,       // _0x | store bytes, ($fp - offset), op

    OP_ITF_MOV, // _01 | itfmov freg, ireg
    OP_FMOV,    // _0x | mov freg, op
    OP_MOV,     // _0x | mov reg, op

    OP_JZ,      // _00 | jz reg immediate  | jump if zero
    OP_JNZ,     // _00 | jnz reg immediate | jump if not zero
    OP_JMP,     // _00 | jmp immediate
    OP_CALL,    // _00 | call immediate
    OP_RET,     // _00 | ret

    OP_EQ,      // _xx | eq op, op
    OP_LT,      // _xx | lt op, op
    OP_GT,      // _xx | gt op, op

    OP_FEQ,     // _xx | feq op, op
    OP_FLT,     // _xx | flt op, op
    OP_FGT,     // _xx | fgt op, op

    OP_AND,     // _xx | and op, op
    OP_OR,      // _xx | or op, op
    OP_NOT,     // _00 | not reg

    OP_NOP,     // _00 | nop

    OP_SYSCALL, // _00 | syscall
    OP_LABEL,   // _00 | label immediate

    VOP_EXIT,   // _00 | exit
};

struct Instruction {
    SageOpCode opcode = SageOpCode::OP_NOP;
    std::array<int64_t, 3> operands = {0, 0, 0};
    int operand_count = 0;

    Instruction();
    Instruction(SageOpCode, int64_t);
    Instruction(SageOpCode, int64_t, int64_t);
    Instruction(SageOpCode, int64_t, int64_t, int64_t);
};

struct Command {
    Instruction instruction;
    AddressMode address_mode = _00;
    // 0 - neutral, use raw immediate
    // 1 - deref register

    Command();
    Command(SageOpCode, int64_t, AddressMode);
    Command(SageOpCode, int64_t, int64_t, AddressMode);
    Command(SageOpCode, int64_t, int64_t, int64_t, AddressMode);

    string print(const map<int, string>* label_names = nullptr);
};

typedef vector<Command> bytecode;
