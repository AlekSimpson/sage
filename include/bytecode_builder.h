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

int hash_djb2(const std::string& str);

struct BytecodeBuilder {
    map<int, procedure_frame> procedures;
    vector<string> procs;
    stack<int> procedure_stack;
    int total_instruction_count = 0;
    bool has_main_function = false;

    BytecodeBuilder();

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
    bytecode final(map<int, int>& proc_locations);
};
