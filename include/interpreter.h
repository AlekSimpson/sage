#pragma once

#include <map>

#include "sage_types.h"
#include "sage_bytecode.h"
#include "symbols.h"

#define LOGIC_REG 21
#define SYSCALL_REG 22
#define STACK_POINTER 23

enum SVM_SYSCALL {
    SYS_WRITE = 0,
    SYS_WRITE_INT,
};

class StackFrame {
public:
    int prog_return_address; // where the program resumes in bytecode after the function finishes executing
    int prog_start_address; // where the function begins in bytecode
    size_t stack_pointer; // keep track of where this frame starts in the stack, so we can pop the frame correctly
    StackFrame *previous_frame;
    map<int, int> saved_caller_values;

    StackFrame(StackFrame *previous,
               map<int, int> caller_cache,
               int ret_addr,
               size_t stack_pointer,
               int prog_start);

    StackFrame();
};


class SageInterpreter {
public:
    // sage argument registers
    // - 0-5  = function parameter registers
    // - 6-9  = return value registers
    // - 10-20 = volatile registers (hold results of temp values and stuff)
    // - 21-24 = system registers
    // - 25-124 = general
    //
    // sr21 = bool logical result register
    // sr22 = syscall register
    // sr23 = stack pointer
    // sr24 = frame pointer
    //
    // opcode   , 0            , 1   , 2
    // sys_write, stdout_fileno, buff, length

    int64_t registers[125];

    bytecode program;
    map<int, int> proc_line_locations;
    SageSymbolTable *symbol_table;
    StackFrame *frame_pointer; // keeps track of current frame

    // memory layout:
    // |--------| <-- array start
    // | static |
    // |--------|
    // | heap   | (grows down)
    // |--------|
    // | free   |
    // |--------|
    // | stack  | (stack grows up)
    // |--------| <-- array end
    vector<uint8_t> memory;
    const size_t static_start_pointer = 0;
    size_t static_memory_end_pointer;
    size_t heap_pointer; // HEAP begins where static ends

    int program_pointer;

    bool vm_running = false;

    SageInterpreter();
    SageInterpreter(SageSymbolTable *table);

    // virtual memory operations
    size_t allocate_on_heap(size_t bytes);
    size_t allocate_on_stack(size_t bytes);
    void push_stack_scope(int func_id);
    void pop_stack_scope(); // pops current stack frame
    inline int stack_pointer();
    double read_float_register(int reg);
    void set_float_register(int reg, double value);

    void open(const map<int, int> &, ByteVector &program_static_memory);
    void close();
    void load_program(bytecode program);
    void execute();
    void print_static_memory();
    void print_stack_memory();

    SageValue get_return_value() const;

    inline void execute_add(std::array<int64_t, 3> &, AddressMode &);
    inline void execute_sub(std::array<int64_t, 3> &, AddressMode &);
    inline void execute_mul(std::array<int64_t, 3> &, AddressMode &);
    inline void execute_div(std::array<int64_t, 3> &, AddressMode &);
    inline void execute_float_add(std::array<int64_t, 3> &);
    inline void execute_float_sub(std::array<int64_t, 3> &);
    inline void execute_float_mul(std::array<int64_t, 3> &);
    inline void execute_float_div(std::array<int64_t, 3> &);
    inline void execute_stack_allocate(int operand);
    inline void execute_load(std::array<int64_t, 3> &, AddressMode &);
    inline void execute_load_reference(std::array<int64_t, 3> &, AddressMode &);
    inline void execute_load_pointer(std::array<int64_t, 3> &, AddressMode &);
    inline void execute_load_address(std::array<int64_t, 3> &, AddressMode &);
    inline void execute_store(std::array<int64_t, 3> &, AddressMode &mode);
    inline void execute_move(std::array<int64_t, 3> &, AddressMode &mode);
    inline void execute_int_to_float_move(std::array<int64_t, 3> &operands);
    inline void execute_float_move(std::array<int64_t, 3> &, AddressMode &mode);
    inline void execute_call(std::array<int64_t, 3> &);
    inline void execute_return();
    inline void execute_equality_comparison(std::array<int64_t, 3> &, AddressMode &mode);
    inline void execute_less_than_comparison(std::array<int64_t, 3> &, AddressMode &mode);
    inline void execute_greater_than_comparison(std::array<int64_t, 3> &, AddressMode &mode);
    inline void execute_and(std::array<int64_t, 3> &, AddressMode &);
    inline void execute_or(std::array<int64_t, 3> &, AddressMode &);
    inline void execute_float_equality_comparison(std::array<int64_t, 3> &, AddressMode &mode);
    inline void execute_float_less_than_comparison(std::array<int64_t, 3> &, AddressMode &mode);
    inline void execute_float_greater_than_comparison(std::array<int64_t, 3> &, AddressMode &mode);
    inline void execute_not(std::array<int64_t, 3> &);
    inline void execute_system_call();
    inline void execute_mem_copy(std::array<int64_t, 3> &, AddressMode &);
    inline void execute_static_copy(std::array<int64_t, 3> &, AddressMode &);
    inline void execute_mem_copy_address(std::array<int64_t, 3> &, AddressMode &);
    inline void execute_store_address(std::array<int64_t, 3> &, AddressMode &);
};
