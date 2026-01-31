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

    void print_bytecode(bytecode &code);
    map<int, ProcedureFrame> &get_active_procedures();
    stack<int> &get_active_procedure_stack();
    int &get_total_instruction_count();
    void increment_total_instruction_count(int delta);

    void enter_comptime();
    void reset_and_exit_comptime();

    void build_instruction(SageOpCode, int, int, int, AddressMode);
    void build_instruction(SageOpCode, int, int, AddressMode);
    void build_instruction(SageOpCode, int, AddressMode);

    void build_builtin_instruction(SageOpCode, int, int, int, AddressMode);
    void build_builtin_instruction(SageOpCode, int, int, AddressMode);
    void build_builtin_instruction(SageOpCode, int, AddressMode);

    void build_fload(int sage_float_register, int offset);
    void build_load(int sage_register, int offset);
    void build_store_immediate(int offset, int immediate);
    void build_store_register(int offset, int sage_register);
    void build_fstore_register(int offset, int float_register);

    void build_fmove_register(int destination_register, int source_register);

    void build_move_immediate(int sage_register, int immediate);
    void build_move_register(int destination_register, int source_register);

    void build_not(int sage_register);

    void build_puti();
    void build_puts();

    void new_frame(string name);
    void exit_frame();
    void reset();

    bytecode finalize_comptime_bytecode(map<int, int> &procedure_line_locations);
    bytecode finalize_runtime_bytecode(map<int, int> &procedure_line_locations);
};
