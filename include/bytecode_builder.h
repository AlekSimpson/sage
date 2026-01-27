#pragma once
#include "sage_bytecode.h"
#include <map>
#include <stack>

struct ProcedureFrame {
    string name;
    bytecode procedure_instructions;

    ProcedureFrame() : name("") {
    }

    ProcedureFrame(string name) : name(name) {
    }
};

int get_procedure_frame_id(const std::string &str);

struct BytecodeBuilder {
    map<int, ProcedureFrame> comptime_procedures;
    map<int, ProcedureFrame> runtime_procedures;
    stack<int> runtime_procedure_stack;
    stack<int> comptime_procedure_stack;
    int comptime_total_instructions = 0;
    int runtime_total_instructions = 0;
    bool runtime_has_main_function = false;
    bool emitting_comptime = false;

    BytecodeBuilder();

    map<int, ProcedureFrame> &get_active_procedures();
    stack<int> &get_active_procedure_stack();
    int &get_total_instruction_count();
    void increment_total_instruction_count(int delta);

    void enter_comptime();
    void reset_and_exit_comptime();

    void build_instruction(SageOpCode, SageValue, SageValue, SageValue);
    void build_instruction(SageOpCode, int, int, SageValue);
    void build_instruction(SageOpCode, int, SageValue, int);
    void build_instruction(SageOpCode, int, int, int);
    void build_instruction(SageOpCode, int, SageValue, SageValue);
    void build_instruction(SageOpCode, SageValue, int, int);
    void build_instruction(SageOpCode, SageValue, int, SageValue);
    void build_instruction(SageOpCode, SageValue, SageValue, int);

    void build_instruction(SageOpCode, int, int);
    void build_instruction(SageOpCode, int, SageValue);
    void build_instruction(SageOpCode, SageValue, int);
    void build_instruction(SageOpCode, SageValue, SageValue);

    void build_instruction(SageOpCode, SageValue);

    void build_puti();
    void build_puts();

    void new_frame(string name);
    void exit_frame();
    void reset();

    bytecode finalize_comptime_bytecode(map<int, int> &procedure_line_locations);
    bytecode finalize_runtime_bytecode(map<int, int> &procedure_line_locations);
};
