#pragma once
#include "sage_bytecode.h"
#include <map>
#include <stack>

struct procedure_frame {
    string name;
    bytecode procedure_instructions;

    procedure_frame() : name("") {
    }

    procedure_frame(string name) : name(name) {
    }
};

struct BytecodeBuilder {
    map<int, procedure_frame> procedures;
    stack<int> procedure_stack;
    // int blank_encoding[4] = {0, 0, 0, 0};
    // int first_encoding[4] = {1, 0, 0, 0};
    int total_instruction_count = 0;

    BytecodeBuilder();

    // void add_instruction(SageOpCode, int);
    // void add_instruction(SageOpCode, int, int (&map)[4]);
    // void add_instruction(SageOpCode, int, int, int (&map)[4]);
    // void add_instruction(SageOpCode, int, int, int, int (&map)[4]);
    // void add_instruction(SageOpCode, int, int, int, int, int (&map)[4]);

    void build_im_im_im(SageOpCode, SageValue, SageValue, SageValue);
    void build_reg_reg_im(SageOpCode, int, int, SageValue);
    void build_reg_im_reg(SageOpCode, int, SageValue, int);
    void build_reg_reg_reg(SageOpCode, int, int, int);
    void build_reg_im_im(SageOpCode, int, SageValue, SageValue);
    void build_im_reg_reg(SageOpCode, SageValue, int, int);
    void build_im_reg_im(SageOpCode, SageValue, int, SageValue);
    void build_im_im_reg(SageOpCode, SageValue, SageValue, int);
    void build_reg_reg(SageOpCode, int, int);
    void build_reg_im(SageOpCode, int, SageValue);
    void build_im_reg(SageOpCode, SageValue, int);
    void build_im_im(SageOpCode, SageValue, SageValue);
    void build_im(SageOpCode, SageValue);

    void new_frame(string name);
    void exit_frame();
    void reset();
    bytecode final();
    int current_id();
};
