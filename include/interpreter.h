#pragma once

#include <map>

#include "sage_types.h"
#include "sage_bytecode.h"
#include "symbols.h"

#define LOGIC_REG 21
#define SYSCALL_REG 22
#define STACK_POINTER 23

#define offset_t int
#define ui64 uint64_t

class StackFrame {
public:
    int prog_return_address; // where the program resumes after the function finishes executing
    int prog_start_address; // where the function begins
    int stack_pointer; // keep track of where this frame starts in the stack, so we can pop the frame correctly
    StackFrame *previous_frame;
    map<int, int> saved_caller_values;
    map<string, offset_t> local_variables; // tracks variable allocations

    StackFrame(StackFrame *previous,
               map<int, int> caller_cache,
               int ret_addr,
               int stack_pointer,
               int prog_start);

    StackFrame();
};


class SageInterpreter {
public:
    // sage argument registers
    // - 0-5  = function parameter registers
    // - 6-9  = return value registers
    // - 10-20 = volatile registers (hold results of temp values and stuff)
    // - 21-23 = system registers
    // - 24-124 = general
    //
    // sr21 = bool logical result register
    // sr22 = syscall register
    // sr23 = stack pointer
    //
    // opcode   , 0            , 1   , 2
    // sys_write, stdout_fileno, buff, length

    SageSymbolTable *symbol_table;

    ui64 registers[125];
    int program_pointer;

    map<int, int> proc_line_locations;
    StackFrame *frame_pointer; // keeps track of current frame

    bytecode program;
    map<int, SageValue> heap;
    vector<SageValue> stack;
    bool vm_running = false;

    SageInterpreter();
    SageInterpreter(SageSymbolTable *table);

    void open(const map<int, int> &, int stack_size);
    void close();
    int store_in_heap(SageValue value);
    void load_program(bytecode program);
    void execute();
    void push_stack_scope(int func_id);
    void pop_stack_scope(); // pops current stack frame
    vector<SageValue> dereference_map(instruction*, int [4]);
    SageValue get_return_value() const;

    void execute_add(vector<SageValue>);
    void execute_sub(vector<SageValue>);
    void execute_mul(vector<SageValue>);
    void execute_div(vector<SageValue>);
    void execute_load(vector<SageValue>);
    void execute_store(vector<SageValue>);
    void execute_mov(vector<SageValue>);
    void execute_call(vector<SageValue>);
    void execute_return();
    void execute_eqcomp(vector<SageValue>);
    void execute_ltcomp(vector<SageValue>);
    void execute_gtcomp(vector<SageValue>);
    void execute_and(vector<SageValue>);
    void execute_or(vector<SageValue>);
    void execute_not(vector<SageValue>);
    void execute_syscall();
};
